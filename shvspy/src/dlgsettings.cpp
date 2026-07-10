#include "dlgsettings.h"
#include "ui_dlgsettings.h"

#include "dlgaddedituser.h"
#include "dlgaddeditmount.h"
#include "dlgaddeditrole.h"
#include "theapp.h"

#include <shv/core/assert.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QMessageBox>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

static const std::string METHOD_VALUE = "value";
static const std::string METHOD_SET_VALUE = "setValue";

DlgSettings::DlgSettings(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &broker_path, const shv::chainpack::IRpcConnection::ShvApiVersion brokerApiVersion, QWidget *parent)
	: Super(parent)
	, ui(new Ui::DlgSettings)
	, m_rpcConnection(rpc_connection)
	, m_brokerPath(broker_path)
	, m_brokerApiVersion(brokerApiVersion)
{
	ui->setupUi(this);

	SHV_ASSERT_EX(rpc_connection != nullptr, "RPC connection is NULL");

	static constexpr double ROW_HEIGHT_RATIO = 1.3;

	//users
	static QStringList USERS_HEADER_NAMES {{ tr("User") }};
	m_usersDataModel = new QStandardItemModel(this);
	m_usersDataModel->setColumnCount(static_cast<int>(USERS_HEADER_NAMES.count()));
	m_usersDataModel->setHorizontalHeaderLabels(USERS_HEADER_NAMES);

	m_usersModelProxy = new QSortFilterProxyModel(this);
	m_usersModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_usersModelProxy->setSourceModel(m_usersDataModel);
	ui->twUsers->setModel(m_usersModelProxy);

	ui->twUsers->horizontalHeader()->setStretchLastSection(true);
	ui->twUsers->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twUsers->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twUsers->verticalHeader()->setVisible(false);
	ui->twUsers->setSortingEnabled(true);
	ui->twUsers->sortByColumn(0, Qt::AscendingOrder);

	connect(ui->pbAddUser, &QPushButton::clicked, this, &DlgSettings::onAddUserClicked);
	connect(ui->pbEditUser, &QPushButton::clicked, this, &DlgSettings::onEditUserClicked);
	connect(ui->pbDeleteUser, &QPushButton::clicked, this, &DlgSettings::onDeleteUserClicked);
	connect(ui->twUsers, &QTableView::doubleClicked, this, &DlgSettings::onEditUserClicked);
	connect(ui->leUsersFilter, &QLineEdit::textChanged, m_usersModelProxy, &QSortFilterProxyModel::setFilterFixedString);

	//roles
	static QStringList ROLES_HEADER_NAMES {{ tr("Role") }};
	m_rolesDataModel = new QStandardItemModel(this);
	m_rolesDataModel->setColumnCount(static_cast<int>(ROLES_HEADER_NAMES.count()));
	m_rolesDataModel->setHorizontalHeaderLabels(ROLES_HEADER_NAMES);

	m_rolesModelProxy = new QSortFilterProxyModel(this);
	m_rolesModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_rolesModelProxy->setSourceModel(m_rolesDataModel);
	ui->twRoles->setModel(m_rolesModelProxy);

	ui->twRoles->horizontalHeader()->setStretchLastSection(true);
	ui->twRoles->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twRoles->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twRoles->verticalHeader()->setVisible(false);
	ui->twRoles->setSortingEnabled(true);
	ui->twRoles->sortByColumn(0, Qt::AscendingOrder);

	connect(ui->pbAddRole, &QPushButton::clicked, this, &DlgSettings::onAddRoleClicked);
	connect(ui->pbEditRole, &QPushButton::clicked, this, &DlgSettings::onEditRoleClicked);
	connect(ui->pbDeleteRole, &QPushButton::clicked, this, &DlgSettings::onDeleteRoleClicked);
	connect(ui->twRoles, &QTableView::doubleClicked, this, &DlgSettings::onEditRoleClicked);
	connect(ui->leRolesFilter, &QLineEdit::textChanged, m_rolesModelProxy, &QSortFilterProxyModel::setFilterFixedString);

	//mounts
	static QStringList MOUNTS_HEADER_NAMES { tr("Device ID"), tr("Mount point"), tr("Description") };
	m_mountsDataModel = new QStandardItemModel(this);
	m_mountsDataModel->setColumnCount(static_cast<int>(MOUNTS_HEADER_NAMES.count()));
	m_mountsDataModel->setHorizontalHeaderLabels(MOUNTS_HEADER_NAMES);

	m_mountsModelProxy = new QSortFilterProxyModel(this);
	m_mountsModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_mountsModelProxy->setSourceModel(m_mountsDataModel);
	m_mountsModelProxy->setFilterKeyColumn(-1);
	ui->twMounts->setModel(m_mountsModelProxy);

	ui->twMounts->horizontalHeader()->setStretchLastSection(true);
	ui->twMounts->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twMounts->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twMounts->verticalHeader()->setVisible(false);
	ui->twMounts->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->twMounts->setSortingEnabled(true);
	ui->twMounts->sortByColumn(0, Qt::AscendingOrder);

	connect(ui->pbAddMount, &QPushButton::clicked, this, &DlgSettings::onAddMountClicked);
	connect(ui->pbEditMount, &QPushButton::clicked, this, &DlgSettings::onEditMountClicked);
	connect(ui->pbDeleteMount, &QPushButton::clicked, this, &DlgSettings::onDeleteMountClicked);
	connect(ui->twMounts, &QTableView::doubleClicked, this, &DlgSettings::onEditMountClicked);
	connect(ui->leMountsFilter, &QLineEdit::textChanged, m_mountsModelProxy, &QSortFilterProxyModel::setFilterFixedString);
	connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::brokerConnectedChanged, this, &DlgSettings::onBrokerConnectedChanged);

	setControlsEnabled(false);
	onBrokerConnectedChanged(m_rpcConnection->isBrokerConnected());
}

DlgSettings::~DlgSettings()
{
	delete ui;
}

void DlgSettings::load()
{
	setStatusText(tr("Loading..."));

	loadUsers([this](bool user_success) {
		if (user_success && m_rpcConnection->isBrokerConnected()) {
			setUserControlsEnabled(true);
		}
		loadRoles([this, user_success](bool role_success) {
			if (role_success && m_rpcConnection->isBrokerConnected()) {
				setRoleControlsEnabled(true);
			}
			loadMounts([this, user_success, role_success](bool mount_success) {
				if (mount_success && m_rpcConnection->isBrokerConnected()) {
					setMountControlsEnabled(true);
				}
				if (user_success && role_success && mount_success && m_rpcConnection->isBrokerConnected()) {
					setStatusText({});
				}
			});
		});
	});
}

std::string DlgSettings::aclAccessPath()
{
	return TheApp::aclAccessPath(m_brokerPath, m_brokerApiVersion);
}

std::string DlgSettings::aclAccessUsersPath()
{
	return aclAccessPath() + "/users";
}

std::string DlgSettings::aclAccessRolesPath()
{
	return aclAccessPath() + "/roles";
}

std::string DlgSettings::aclAccessMountsPath()
{
	return aclAccessPath() + "/mounts";
}

void DlgSettings::setStatusText(const QString &txt)
{
	if (txt.isEmpty()) {
		ui->lblStatus->hide();
	}
	else {
		ui->lblStatus->show();
		ui->lblStatus->setText(txt);
	}
}

void DlgSettings::onBrokerConnectedChanged(bool is_connected)
{
	if (is_connected) {
		clearUsers();
		clearRoles();
		clearMounts();
		load();
	}
	else {
		setControlsEnabled(false);
		setStatusText(tr("Broker disconnected."));
	}
}

void DlgSettings::loadUsers(std::function<void (bool)> callback)
{
	callShvMethod(aclAccessUsersPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isList()) {
			const auto &res = result.asList();
			m_usersDataModel->setRowCount(static_cast<int>(res.size()));
			for (size_t i = 0; i < res.size(); i++) {
				auto *item = new QStandardItem(res.at(i).to<QString>());
				item->setFlags(item->flags() & ~Qt::ItemIsEditable);
				m_usersDataModel->setItem(static_cast<int>(i), 0, item);
			}
			sortTable(ui->twUsers);
			callback(true);
		}
		else {
			setStatusText(tr("Invalid response from server."));
			callback(false);
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to load users.") + " " + error);
		callback(false);
	});
}

void DlgSettings::reloadUsers()
{
	QString last_current_row = currentRow(ui->twUsers);
	clearUsers();
	setUserControlsEnabled(false);
	setStatusText(tr("Reloading users..."));
	loadUsers([this, last_current_row](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setUserControlsEnabled(true);
			setCurrentRow(ui->twUsers, last_current_row);
			setStatusText({});
		}
	});
}

void DlgSettings::clearUsers()
{
	m_usersDataModel->removeRows(0, m_usersDataModel->rowCount());
}

void DlgSettings::onAddUserClicked()
{
	auto *dlg = new DlgAddEditUser(m_rpcConnection, aclAccessPath(), {}, this);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			reloadUsers();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::onEditUserClicked()
{
	QString user = currentRow(ui->twUsers);
	if (user.isEmpty()) {
		setStatusText(tr("Select user in the table."));
		return;
	}

	setStatusText({});

	auto *dlg = new DlgAddEditUser(m_rpcConnection, aclAccessPath(), user, this);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted) {
			reloadUsers();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::onDeleteUserClicked()
{
	QString user = currentRow(ui->twUsers);
	if (user.isEmpty()){
		setStatusText(tr("Select user in the table."));
		return;
	}

	if (QMessageBox::question(this, tr("Delete user"), tr("Do you really want to delete user %1?").arg(user)) == QMessageBox::Yes){
		callShvMethod(aclAccessUsersPath(), METHOD_SET_VALUE, shv::chainpack::RpcValue::List{user.toStdString(), {}}, [this](const shv::chainpack::RpcValue &) {
			reloadUsers();
		}, [this](const QString &error) {
			setStatusText(tr("Failed to delete user. Error:") + " " + error);
		});
	}
}

void DlgSettings::clearRoles()
{
	m_rolesDataModel->removeRows(0, m_rolesDataModel->rowCount());
}

void DlgSettings::loadRoles(std::function<void (bool)> callback)
{
	callShvMethod(aclAccessRolesPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isList()){
			const auto &res = result.asList();
			m_rolesDataModel->setRowCount(static_cast<int>(res.size()));
			for (size_t i = 0; i < res.size(); i++){
				auto *item = new QStandardItem(QString::fromStdString(res.at(i).asString()));
				item->setFlags(item->flags() & ~Qt::ItemIsEditable);
				m_rolesDataModel->setItem(static_cast<int>(i), 0, item);
			}
			sortTable(ui->twRoles);
			callback(true);
		}
		else {
			setStatusText(tr("Invalid response from server."));
			callback(false);
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to load roles.") + " " + error);
		callback(false);
	});
}

void DlgSettings::reloadRoles()
{
	QString last_current_row = currentRow(ui->twRoles);
	clearRoles();
	setRoleControlsEnabled(false);
	setStatusText(tr("Reloading roles..."));
	loadRoles([this, last_current_row](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setRoleControlsEnabled(true);
			setCurrentRow(ui->twRoles, last_current_row);
			setStatusText({});
		}
	});
}

void DlgSettings::onAddRoleClicked()
{
	auto *dlg = new DlgAddEditRole(m_rpcConnection, aclAccessPath(), {}, this);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			reloadRoles();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::onEditRoleClicked()
{
	QString role = currentRow(ui->twRoles);
	if (role.isEmpty()){
		setStatusText(tr("Select role in the table."));
		return;
	}

	auto *dlg = new DlgAddEditRole(m_rpcConnection, aclAccessPath(), role, this);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			reloadRoles();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::onDeleteRoleClicked()
{
	QString role = currentRow(ui->twRoles);
	if (role.isEmpty()){
		setStatusText(tr("Select role in the table."));
		return;
	}

	if (QMessageBox::question(this, tr("Delete role"), tr("Do you really want to delete role %1?").arg(role)) == QMessageBox::Yes){
		callShvMethod(aclAccessRolesPath(), METHOD_SET_VALUE, shv::chainpack::RpcValue::List{role.toStdString(), {}}, [this](const shv::chainpack::RpcValue &) {
			reloadRoles();
		}, [this](const QString &error) {
			setStatusText(tr("Failed to delete role. Error: ") + " " + error);
		});
	}
}

void DlgSettings::setUserControlsEnabled(bool enabled)
{
	ui->pbAddUser->setEnabled(enabled);
	ui->pbEditUser->setEnabled(enabled);
	ui->pbDeleteUser->setEnabled(enabled);
	ui->leUsersFilter->setEnabled(enabled);
	ui->twUsers->setEnabled(enabled);
}

void DlgSettings::setRoleControlsEnabled(bool enabled)
{
	ui->pbAddRole->setEnabled(enabled);
	ui->pbEditRole->setEnabled(enabled);
	ui->pbDeleteRole->setEnabled(enabled);
	ui->leRolesFilter->setEnabled(enabled);
	ui->twRoles->setEnabled(enabled);
}

void DlgSettings::setMountControlsEnabled(bool enabled)
{
	ui->pbAddMount->setEnabled(enabled);
	ui->pbEditMount->setEnabled(enabled);
	ui->pbDeleteMount->setEnabled(enabled);
	ui->leMountsFilter->setEnabled(enabled);
	ui->twMounts->setEnabled(enabled);
}

void DlgSettings::setControlsEnabled(bool enabled)
{
	setUserControlsEnabled(enabled);
	setRoleControlsEnabled(enabled);
	setMountControlsEnabled(enabled);
}

QString DlgSettings::currentRow(QTableView *table) const
{
	QModelIndex current_index = table->currentIndex();
	if (current_index.isValid()) {
		return table->model()->index(current_index.row(), 0).data().toString();
	}
	return {};
}

void DlgSettings::setCurrentRow(QTableView *table, const QString &row)
{
	if (!row.isEmpty()) {
		auto *model = table->model();
		for (int i = 0; i < model->rowCount(); ++i) {
			if (model->index(i, 0).data().toString() == row) {
				table->setCurrentIndex(model->index(i, 0));
			}
		}
	}
}

void DlgSettings::sortTable(QTableView *table) const
{
	table->sortByColumn(table->horizontalHeader()->sortIndicatorSection(), table->horizontalHeader()->sortIndicatorOrder());
}

void DlgSettings::clearMounts()
{
	m_mountsDataModel->removeRows(0, m_mountsDataModel->rowCount());
}

void DlgSettings::loadMounts(std::function<void (bool)> callback)
{
	callShvMethod(aclAccessMountsPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (!result.isList()) {
			setStatusText(tr("Invalid response from server."));
			callback(false);
			return;
		}
		auto result_list = result.asList();
		if (result_list.empty()) {
			callback(true);
			return;
		}
		QSharedPointer<int> unfinished_calls(new int(static_cast<int>(result_list.size())));
		QSharedPointer<bool> success(new bool(true));

		auto on_call_finished = [this, unfinished_calls, callback, success]() {
			if (--(*unfinished_calls) == 0) {
				if (!(*success)) {
					clearMounts();
				}
				else {
					sortTable(ui->twMounts);
				}
				callback(*success);
			}
		};

		for (const auto &mount : result_list){
			QString mount_id = mount.to<QString>();
			callShvMethod(aclAccessMountsPath() + "/" + mount.asString(), METHOD_VALUE, {}, [this, mount_id, on_call_finished, success](const shv::chainpack::RpcValue &result) {
				if (!result.isMap()) {
					(*success) = false;
					setStatusText(tr("Invalid response from server."));
					on_call_finished();
					return;
				}
				int row = m_mountsDataModel->rowCount();
				m_mountsDataModel->setRowCount(row + 1);

				auto *id_item = new QStandardItem(mount_id);
				id_item->setFlags(id_item->flags() & ~Qt::ItemIsEditable);
				m_mountsDataModel->setItem(row, 0, id_item);

				auto *mountpoint_item = new QStandardItem(result.at("mountPoint").to<QString>());
				mountpoint_item->setFlags(mountpoint_item->flags() & ~Qt::ItemIsEditable);
				m_mountsDataModel->setItem(row, 1, mountpoint_item);

				auto *description_item = new QStandardItem(result.at("description").to<QString>());
				description_item->setFlags(description_item->flags() & ~Qt::ItemIsEditable);
				m_mountsDataModel->setItem(row, 2, description_item);

				on_call_finished();
			}, [this, mount_id, on_call_finished, success](const QString &error) {
				setStatusText(tr("Failed to load mountpoint definition for ID: ") + mount_id + " - " + error);
				(*success) = false;
				on_call_finished();
			});
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to load mount point list.") + " " + error);
		callback(false);
	});
}

void DlgSettings::reloadMounts()
{
	QString last_current_row = currentRow(ui->twMounts);
	clearMounts();
	setMountControlsEnabled(false);
	setStatusText(tr("Reloading mount points..."));
	loadMounts([this, last_current_row](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setMountControlsEnabled(true);
			setCurrentRow(ui->twMounts, last_current_row);
			setStatusText({});
		}
	});
}

void DlgSettings::onAddMountClicked()
{
	auto *dlg = new DlgAddEditMount(this, m_rpcConnection, aclAccessPath(), DlgAddEditMount::DialogType::Add);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			reloadMounts();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::onDeleteMountClicked()
{
	QString mount = currentRow(ui->twMounts);
	if (mount.isEmpty()){
		setStatusText(tr("Select mount point in the table."));
		return;
	}

	if (QMessageBox::question(this, tr("Delete mount"), tr("Do you really want to delete mount point %1?").arg(mount)) == QMessageBox::Yes){
		callShvMethod(aclAccessMountsPath(), METHOD_SET_VALUE, shv::chainpack::RpcValue::List{mount.toStdString(), {}}, [this](const shv::chainpack::RpcValue &) {
			reloadMounts();
		}, [this](const QString &error) {
			setStatusText(tr("Failed to delete mount definition.") + " " + error);
		});
	}
}

void DlgSettings::onEditMountClicked()
{
	QString mount = currentRow(ui->twMounts);
	if (mount.isEmpty()){
		setStatusText(tr("Select mount point in the table."));
		return;
	}

	auto *dlg = new DlgAddEditMount(this, m_rpcConnection, aclAccessPath(), DlgAddEditMount::DialogType::Edit);
	dlg->init(mount);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			reloadMounts();
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::callShvMethod(const std::string &path, const std::string &method, const shv::chainpack::RpcValue &params, std::function<void(const shv::chainpack::RpcValue &)> on_success, std::function<void(const QString &)> on_error)
{
	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);
	cb->start(this, [on_success, on_error](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()) {
			if (response.isError()) {
				on_error(QString::fromStdString(response.error().toString()));
			}
			else {
				on_success(response.result());
			}
		}
		else {
			on_error(tr("Request timeout expired"));
		}
	});
	m_rpcConnection->callShvMethod(rqid, path, method, params);
}
