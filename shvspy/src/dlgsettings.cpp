#include "dlgsettings.h"
#include "ui_dlgsettings.h"

#include "accessmodel/accessmodelshv3.h"
#include "accessmodel/accessmodelshv2.h"
#include "accessmodel/accessitemdelegateshv2.h"
#include "theapp.h"
#include "dlgselectroles.h"

#include <shv/core/assert.h>
#include <shv/iotqt/acl/aclmountdef.h>
#include <shv/iotqt/acl/aclrole.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QComboBox>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

static const std::string METHOD_VALUE = "value";
static const std::string METHOD_SET_VALUE = "setValue";

class RoleDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	struct RoleItem {
		QString name;
		QString description;
	};
	QList<RoleItem> roles = {
		{ shv::chainpack::Rpc::ROLE_BROWSE, QT_TRANSLATE_NOOP("", "Browse") },
		{ shv::chainpack::Rpc::ROLE_READ, QT_TRANSLATE_NOOP("", "Read") },
		{ shv::chainpack::Rpc::ROLE_WRITE, QT_TRANSLATE_NOOP("", "Write") },
		{ shv::chainpack::Rpc::ROLE_COMMAND, QT_TRANSLATE_NOOP("", "Command") },
		{ shv::chainpack::Rpc::ROLE_CONFIG, QT_TRANSLATE_NOOP("", "Config") },
		{ shv::chainpack::Rpc::ROLE_SERVICE, QT_TRANSLATE_NOOP("", "Service") },
		{ shv::chainpack::Rpc::ROLE_SUPER_SERVICE, QT_TRANSLATE_NOOP("", "Super service") },
		{ shv::chainpack::Rpc::ROLE_DEVEL, QT_TRANSLATE_NOOP("", "Developer") },
		{ shv::chainpack::Rpc::ROLE_ADMIN, QT_TRANSLATE_NOOP("", "Administrator") },
	};


	QWidget *createEditor(QWidget *parent,  const QStyleOptionViewItem &, const QModelIndex &) const override
	{
		auto *editor = new QComboBox(parent);
		for (const auto &role : roles) {
			editor->addItem(role.description + " (" + role.name + ")", role.name);
		}
		return editor;
	}

	void setEditorData(QWidget *editor, const QModelIndex &index) const override
	{
		QVariant value = index.model()->data(index);
		auto *combobox = static_cast<QComboBox*>(editor);

		int idx = combobox->findData(value);
		if (idx >= 0) {
			combobox->setCurrentIndex(idx);
		}
	}

	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
	{
		auto *comboBox = static_cast<QComboBox*>(editor);
		model->setData(index, comboBox->currentData());
	}
};

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

	connect(ui->tbShowPassword, &QToolButton::clicked, this, [this](){
		setUserPasswordMode(ui->leUserPassword->echoMode() != QLineEdit::EchoMode::Password);
	});
	connect(ui->pbSelectUserRoles, &QPushButton::clicked, this, &DlgSettings::onSelectRolesClicked);
	hideUserEdit();
	connect(ui->editUserButtonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
		if (button == ui->editUserButtonBox->button(QDialogButtonBox::Save)) {
			auto user_to_select = ui->leUserName->text().trimmed();
			ui->editUserWidget->setEnabled(false);
			saveUserEdit([this, user_to_select](bool success) {
				ui->editUserWidget->setEnabled(true);
				if (success) {
					hideUserEdit();
					reloadUsers(user_to_select);
				}
			});
		}
		else {
			hideUserEdit();
		}
	});

	if (isShv3()) {
		m_accessModel = new AccessModelShv3(this);
		ui->tvAccessRules->setItemDelegateForColumn(AccessModelShv3::ColGrant, new RoleDelegate(this));
	}
	else {
		m_accessModel = new AccessModelShv2(this);
		ui->tvAccessRules->setItemDelegate(new AccessItemDelegateShv2(ui->tvAccessRules));
	}
	ui->tvAccessRules->setModel(m_accessModel);
	ui->tvAccessRules->verticalHeader()->setDefaultSectionSize(static_cast<int>(fontMetrics().height() * 1.3));
	connect(ui->tbAddRow, &QToolButton::clicked, m_accessModel, &AccessModel::addRule);
	connect(ui->tbDeleteRow, &QToolButton::clicked, this, [this]() {
		m_accessModel->deleteRule(ui->tvAccessRules->currentIndex().row());
	});
	connect(ui->tbMoveRowUp, &QToolButton::clicked, this, [this]() {
		m_accessModel->moveRuleUp(ui->tvAccessRules->currentIndex().row());
	});
	connect(ui->tbMoveRowDown, &QToolButton::clicked, this, [this]() {
		m_accessModel->moveRuleDown(ui->tvAccessRules->currentIndex().row());
	});
	connect(ui->pbSelectRoles, &QPushButton::clicked, this, [this]() {
		execSelectRolesDialog(ui->leRoles);
	});
	hideRoleEdit();
	connect(ui->editRoleButtonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
		if (button == ui->editRoleButtonBox->button(QDialogButtonBox::Save)) {
			auto role_to_select = ui->leRoleName->text().trimmed();
			ui->editRoleWidget->setEnabled(false);
			saveRoleEdit([this, role_to_select](bool success) {
				ui->editRoleWidget->setEnabled(true);
				if (success) {
					hideRoleEdit();
					reloadRoles(role_to_select);
				}
			});
		}
		else {
			hideRoleEdit();
		}
	});

	hideMountEdit();
	connect(ui->editMountButtonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
		if (button == ui->editMountButtonBox->button(QDialogButtonBox::Save)) {
			auto mount_point_to_select = ui->leMountDeviceId->text().trimmed();
			ui->editMountWidget->setEnabled(false);
			saveMountEdit([this, mount_point_to_select](bool success) {
				ui->editMountWidget->setEnabled(true);
				if (success) {
					hideMountEdit();
					reloadMounts(mount_point_to_select);
				}
			});
		}
		else {
			hideMountEdit();
		}
	});

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

bool DlgSettings::isShv3() const
{
	return m_brokerApiVersion == shv::chainpack::IRpcConnection::ShvApiVersion::V3;
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
		hideUserEdit();
		hideRoleEdit();
		hideMountEdit();
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

void DlgSettings::reloadUsers(const QString &user_to_select)
{
	clearUsers();
	setUserControlsEnabled(false);
	setStatusText(tr("Reloading users..."));
	loadUsers([this, user_to_select](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setUserControlsEnabled(true);
			setCurrentRow(ui->twUsers, user_to_select);
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
	showUserEdit();
	m_editUser = {};
	ui->editUserWidget->setTitle(tr("New user"));
	ui->leUserName->setReadOnly(false);
	ui->leUserName->setFocus();
}

void DlgSettings::onEditUserClicked()
{
	QString user = currentRow(ui->twUsers);
	if (user.isEmpty()) {
		setStatusText(tr("Select user in the table."));
		return;
	}
	setUserControlsEnabled(false);
	callGetUser([this, user](bool success) {
		const bool connected = m_rpcConnection->isBrokerConnected();
		setUserControlsEnabled(connected);
		if (!success || !connected) {
			return;
		}

		showUserEdit();
		ui->editUserWidget->setTitle(tr("Edit user"));
		ui->leUserName->setReadOnly(true);
		ui->leUserName->setText(user);
		QStringList roles;
		for (const auto &role : m_editUser.roles) {
			roles << QString::fromStdString(role);
		}
		setStringListToLineEdit(ui->leUserRoles, roles);
	});
}

void DlgSettings::showUserEdit()
{
	ui->twUsers->setEnabled(false);
	ui->userControlsWidget->hide();
	ui->leUserName->clear();
	ui->leUserPassword->clear();
	ui->leUserRoles->clear();
	ui->chbCreateRole->setChecked(false);
	setUserPasswordMode(true);
	ui->editUserWidget->show();
	ui->editUserWidget->setEnabled(true);
	ui->leUsersFilter->setEnabled(false);
}

void DlgSettings::hideUserEdit()
{
	ui->editUserWidget->hide();
	ui->userControlsWidget->show();
	ui->twUsers->setEnabled(true);
	ui->leUsersFilter->setEnabled(true);
}

void DlgSettings::hideMountEdit()
{
	ui->editMountWidget->hide();
	ui->mountControlsWidget->show();
	ui->twMounts->setEnabled(true);
	ui->leMountsFilter->setEnabled(true);
}

void DlgSettings::hideRoleEdit()
{
	ui->editRoleWidget->hide();
	ui->roleControlsWidget->show();
	ui->twRoles->setEnabled(true);
	ui->leRolesFilter->setEnabled(true);
}

void DlgSettings::saveRoleEdit(std::function<void (bool)> callback)
{
	if (ui->leRoleName->text().isEmpty()){
		setStatusText(tr("Role name is empty."));
		callback(false);
		return;
	}
	if (!m_accessModel->isRulesValid()){
		setStatusText(tr("Access rules are invalid."));
		callback(false);
		return;
	}
	if (ui->leRoleName->isReadOnly()) {
		setStatusText(tr("Updating role..."));
		callSaveRole(callback);
	}
	else {
		setStatusText(tr("Checking role existence..."));
		checkExistingRole([this, callback](bool success, bool is_duplicate) {
			if (!success) {
				callback(false);
				return;
			}
			if (is_duplicate) {
				setStatusText(tr("Cannot add role, name is duplicate!"));
				callback(false);
				return;
			}
			setStatusText(tr("Adding new role..."));
			callSaveRole(callback);
		});
	}
}

void DlgSettings::callGetRole(std::function<void (bool, const QStringList &, const shv::chainpack::RpcValue &, const std::optional<int> &, const shv::chainpack::RpcValue &)> callback)
{
	QString role = currentRow(ui->twRoles);
	auto role_path = aclAccessRolesPath() + '/' + role.toStdString();
	auto on_error = [this, callback](const QString &error) {
		setStatusText(tr("Failed to get role definition.") + " " + error);
		callback(false, {}, {}, {}, {});
	};

	if (isShv3()) {
		callShvMethod(role_path, METHOD_VALUE, {}, [callback](const shv::chainpack::RpcValue &result) {
			const auto &role_map = result.asMap();
			QStringList roles;
			for (const auto &role : role_map.valref("roles").asList()) {
				roles << role.to<QString>();
			}
			callback(true, roles, role_map.value("profile"), {}, role_map.valref("access"));
		}, on_error);
	}
	else {
		callShvMethod(aclAccessRolesPath() + '/' + role.toStdString(), METHOD_VALUE, {}, [this, role, callback, on_error](const shv::chainpack::RpcValue &role_result) {
			auto acl_role = shv::iotqt::acl::AclRole::fromRpcValue(role_result).value_or(shv::iotqt::acl::AclRole());
			QStringList roles;
			for (const auto &role : acl_role.roles) {
				roles << QString::fromStdString(role);
			}
			auto profile = acl_role.profile;
			auto w = role_result.asMap().value("weight");
			std::optional<int> weight = w.isInt() ? std::optional<int>(w.toInt()) : std::nullopt;
			callShvMethod(aclAccessPath() + "/access/" + role.toStdString(), METHOD_VALUE, {}, [callback, roles, profile, weight](const shv::chainpack::RpcValue &access_result) {
				callback(true, roles, profile, weight, access_result);
			}, on_error);
		}, on_error);
	}
}

void DlgSettings::callSaveRole(std::function<void (bool)> callback)
{
	auto role = ui->leRoleName->text().trimmed();
	setStatusText(tr("Saving role %1...").arg(role));
	std::vector<std::string> roles;
	for (const auto &role : stringListFromLineEdit(ui->leRoles)) {
		roles.push_back(role.toStdString());
	}
	shv::chainpack::RpcValue profile;
	std::string profile_string = ui->leProfile->text().trimmed().toStdString();
	if (!profile_string.empty()) {
		std::string err;
		profile = shv::chainpack::RpcValue::fromCpon(profile_string, &err);
		if (!err.empty()) {
			setStatusText(tr("Invalid profile definition"));
			callback(false);
			return;
		}
	}

	if (isShv3()) {
		shv::chainpack::RpcValue::Map role_rpc {
			{ "roles", roles },
			{ "access", m_accessModel->rules() },
			{ "profile", profile },
		};

		shv::chainpack::RpcValue::List params{ role.toStdString(), role_rpc };
		callShvMethod(aclAccessRolesPath(), METHOD_SET_VALUE, params, [callback](const shv::chainpack::RpcValue &) {
			callback(true);
		}, [this, callback](const QString &error) {
			setStatusText(tr("Failed to save role.") + " " + error);
			callback(false);
		});
	}
	else {
		shv::iotqt::acl::AclRole acl_role;
		acl_role.roles = roles;
		acl_role.profile = profile;

		auto role_rpc = acl_role.toRpcValue();
		if (ui->sbWeight->isVisible()) {
			role_rpc.set("weight", ui->sbWeight->value());
		}

		shv::chainpack::RpcValue::List params{ role.toStdString(), role_rpc };
		callShvMethod(aclAccessRolesPath(), METHOD_SET_VALUE, params, [this, role, callback](const shv::chainpack::RpcValue &) {
			shv::chainpack::RpcValue::List params{ role.toStdString(), m_accessModel->rules() };
			callShvMethod(aclAccessPath() + "/access", METHOD_SET_VALUE, params, [callback](const shv::chainpack::RpcValue &) {
				callback(true);
			}, [this, callback](const QString &error) {
				setStatusText(tr("Failed to save access rules.") + " " + error);
				callback(false);
			});
		}, [this, callback](const QString &error) {
			setStatusText(tr("Failed to save role.") + " " + error);
			callback(false);
		});
	}
}

void DlgSettings::checkExistingRole(std::function<void (bool, bool)> callback)
{
	callShvMethod(aclAccessRolesPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isList()) {
			std::string role = ui->leRoleName->text().trimmed().toStdString();
			for (const auto &item : result.asList()) {
				if (item.asString() == role) {
					callback(true, true);
					return;
				}
			}
			callback(true, false);
		}
		else {
			setStatusText(tr("Failed to check role. Bad server response format."));
			callback(false, false);
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to check role.") + " " + error);
		callback(false, false);
	});
}

void DlgSettings::callGetMount(std::function<void (bool, const shv::iotqt::acl::AclMountDef &)> callback)
{
	QString mount = currentRow(ui->twMounts);
	callShvMethod(aclAccessMountsPath() + '/' + mount.toStdString(), METHOD_VALUE, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isMap()) {
			callback(true, shv::iotqt::acl::AclMountDef::fromRpcValue(result));
		}
		else {
			setStatusText(tr("Invalid response from server."));
			callback(false, {});
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to get mount definition.") + " " + error);
		callback(false, {});
	});
}

void DlgSettings::callSaveMount(std::function<void (bool)> callback)
{
	auto device_id = ui->leMountDeviceId->text().trimmed();
	auto mount_point = ui->leMountPoint->text().trimmed();
	auto description = ui->leMountDescription->text().trimmed();

	if (device_id.isEmpty()){
		setStatusText(tr("Error: device id is empty."));
		callback(false);
		return;
	}
	if (mount_point.isEmpty()) {
		setStatusText(tr("Error: mount point is empty."));
		callback(false);
		return;
	}

	shv::iotqt::acl::AclMountDef mount_def { mount_point.toStdString(), description.toStdString() };
	callShvMethod(aclAccessMountsPath(), METHOD_SET_VALUE, shv::chainpack::RpcValue::List{device_id.toStdString(), mount_def.toRpcValue()}, [callback](const shv::chainpack::RpcValue &) {
		callback(true);
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to save mount definition.") + " " + error);
		callback(false);
	});
}

void DlgSettings::checkExistingMount(std::function<void (bool, bool)> callback)
{
	callShvMethod(aclAccessMountsPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isList()) {
			std::string mount_id = ui->leMountDeviceId->text().trimmed().toStdString();
			for (const auto &item : result.asList()) {
				if (item.asString() == mount_id) {
					callback(true, true);
					return;
				}
			}
			callback(true, false);
		}
		else {
			setStatusText(tr("Failed to check mount ID. Bad server response format."));
			callback(false, false);
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to check mount ID.") + " " + error);
		callback(false, false);
	});
}

void DlgSettings::saveMountEdit(std::function<void (bool)> callback)
{
	if (ui->leMountDeviceId->isReadOnly()) {
		setStatusText(tr("Updating mount point..."));
		callSaveMount(callback);
	}
	else {
		if (ui->leMountDeviceId->text().isEmpty() || ui->leMountPoint->text().isEmpty()) {
			setStatusText(tr("Device id or mount point is empty."));
			callback(false);
			return;
		}
		setStatusText(tr("Checking mount point existence..."));
		checkExistingMount([this, callback](bool success, bool is_duplicate) {
			if (!success) {
				callback(false);
				return;
			}
			if (is_duplicate) {
				setStatusText(tr("Cannot add mount point, device id is duplicate!"));
				callback(false);
				return;
			}
			setStatusText(tr("Adding new mount point..."));
			callSaveMount(callback);
		});
	}
}

void DlgSettings::showMountEdit()
{
	ui->twMounts->setEnabled(false);
	ui->mountControlsWidget->hide();
	ui->leMountDeviceId->clear();
	ui->leMountPoint->clear();
	ui->leMountDescription->clear();
	ui->editMountWidget->show();
	ui->editMountWidget->setEnabled(true);
	ui->leMountsFilter->setEnabled(false);
}

void DlgSettings::showRoleEdit()
{
	ui->twRoles->setEnabled(false);
	ui->roleControlsWidget->hide();
	ui->leRoleName->clear();
	ui->leRoles->clear();
	ui->sbWeight->clear();
	ui->leProfile->clear();
	m_accessModel->setRules({});
	ui->editRoleWidget->show();
	ui->editRoleWidget->setEnabled(true);
	ui->leRolesFilter->setEnabled(false);
	ui->lblWeight->setVisible(!isShv3());
	ui->sbWeight->setVisible(!isShv3());
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
			reloadUsers({});
		}, [this](const QString &error) {
			setStatusText(tr("Failed to delete user. Error:") + " " + error);
		});
	}
}

void DlgSettings::setUserPasswordMode(bool password_mode)
{
	ui->leUserPassword->setEchoMode((password_mode) ? QLineEdit::EchoMode::Password : QLineEdit::EchoMode::Normal);
	ui->tbShowPassword->setIcon((password_mode) ? QIcon(":/shvspy/images/show.svg") : QIcon(":/shvspy/images/hide.svg"));
}

void DlgSettings::onSelectRolesClicked()
{
	if (ui->chbCreateRole->isChecked() && !ui->leUserName->text().isEmpty()){
		if (QMessageBox::question(this, tr("Confirm create role"),
								  tr("You are requesting create new role. So you can select roles properly, "
									 "new role must be created now. It will not be deleted if you cancel this dialog. "
									 "Do you want to continue?")) == QMessageBox::StandardButton::Yes){
			ui->editUserWidget->setEnabled(false);
			callCreateRole([this](bool success){
				ui->editUserWidget->setEnabled(true);
				if (success) {
					execSelectRolesDialog(ui->leUserRoles);
				}
			});
		}
	}
	else {
		execSelectRolesDialog(ui->leUserRoles);
	}
}

QStringList DlgSettings::stringListFromLineEdit(QLineEdit *le) const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	auto skip_empty_parts = QString::SkipEmptyParts;
#else
	auto skip_empty_parts = Qt::SkipEmptyParts;
#endif
	QStringList items;
	for (const auto &item : le->text().split(",", skip_empty_parts)) {
		items << item.trimmed();
	}
	return items;
}

void DlgSettings::setStringListToLineEdit(QLineEdit *le, const QStringList &items)
{
	le->setText(items.join(','));
}

void DlgSettings::execSelectRolesDialog(QLineEdit *le)
{
	auto *dlg = new DlgSelectRoles(this);
	dlg->init(m_rpcConnection, aclAccessPath(), stringListFromLineEdit(le));
	connect(dlg, &QDialog::finished, dlg, [this, dlg, le] (int result) {
		if (result == QDialog::Accepted){
			setStringListToLineEdit(le, dlg->selectedRoles());
		}
		dlg->deleteLater();
	});
	dlg->open();
}

void DlgSettings::callCreateRole(std::function<void (bool)> callback)
{
	auto role_name = ui->leUserName->text();
	auto role = shv::iotqt::acl::AclRole().toRpcValue();
	role.set("weight", 0);

	callShvMethod(aclAccessRolesPath(), METHOD_SET_VALUE, shv::chainpack::RpcValue::List{role_name.toStdString(), role}, [this, role_name, callback](const auto &) {
		auto roles = stringListFromLineEdit(ui->leUserRoles);
		roles << role_name;
		std::sort(roles.begin(), roles.end());
		setStringListToLineEdit(ui->leUserRoles, roles);
		callback(true);
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to add role.") + " " + error);
		callback(false);
	});
}

void DlgSettings::saveUserEdit(std::function<void(bool)> callback)
{
	auto do_save = [this, callback]() {
		if (ui->chbCreateRole->isChecked() && !stringListFromLineEdit(ui->leUserRoles).contains(ui->leUserName->text())) {
			callCreateRole([this, callback](bool success){
				if (success) {
					callSaveUser(callback);
				}
				else {
					callback(false);
				}
			});
		}
		else {
			callSaveUser(callback);
		}
	};
	if (ui->leUserName->isReadOnly()) {
		setStatusText(tr("Updating user..."));
		do_save();
	}
	else {
		if (ui->leUserName->text().isEmpty() || ui->leUserPassword->text().isEmpty()) {
			setStatusText(tr("User name or password is empty."));
			callback(false);
			return;
		}
		setStatusText(tr("Checking user name existence..."));
		checkExistingUser([this, callback, do_save](bool success, bool is_duplicate) {
			if (!success) {
				callback(false);
				return;
			}
			if (is_duplicate) {
				setStatusText(tr("Cannot add user, user name is duplicate!"));
				callback(false);
				return;
			}
			setStatusText(tr("Adding new user..."));
			do_save();
		});
	}
}

namespace {
constexpr auto PASSWORD = "password";
constexpr auto PLAIN = "Plain";
constexpr auto SHA1 = "Sha1";

shv::iotqt::acl::AclUser shv3AclUserFromRpcValue(const shv::chainpack::RpcValue &v)
{
	// SHV3
	// {
	//   "password":{"Plain":"viewer"},
	//   "roles":["subscribe", "browse"]
	// }
	using namespace shv::iotqt::acl;
	AclUser ret;
	const auto &m = v.asMap();
	{
		const auto &pass = m.valref(PASSWORD).asMap();
		if (pass.hasKey(SHA1)) {
			ret.password.password = pass.value(SHA1).asString();
			ret.password.format = AclPassword::Format::Sha1;
		}
		else if (pass.hasKey(PLAIN)) {
			ret.password.password = pass.value(PLAIN).asString();
			ret.password.format = AclPassword::Format::Plain;
		}
	}
	std::vector<std::string> roles;
	for(const auto &lst : m.valref("roles").asList()) {
		roles.push_back(lst.toString());
	}
	ret.roles = roles;
	return ret;
}

shv::chainpack::RpcValue shv3AclUserToRpcValue(const shv::iotqt::acl::AclUser &user)
{
	using namespace shv::iotqt::acl;
	shv::chainpack::RpcValue::Map ret;
	switch (user.password.format) {
	case AclPassword::Format::Invalid:
		break;
	case AclPassword::Format::Plain:
		ret[PASSWORD] = shv::chainpack::RpcValue::Map{{PLAIN, user.password.password}};
		break;
	case AclPassword::Format::Sha1:
		ret[PASSWORD] = shv::chainpack::RpcValue::Map{{SHA1, user.password.password}};
		break;
	}
	ret["roles"] = user.roles;
	return ret;
}

std::string sha1_hex(const std::string &s)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Sha1);
#if QT_VERSION_MAJOR >= 6 && QT_VERSION_MINOR >= 3
	hash.addData(QByteArrayView(s.data(), s.length()));
#else
	hash.addData(s.data(), s.length());
#endif
	return std::string(hash.result().toHex().constData());
}
}

void DlgSettings::callGetUser(std::function<void(bool)> callback)
{
	setStatusText(tr("Getting user roles..."));
	auto user = currentRow(ui->twUsers).toStdString();

	callShvMethod(aclAccessUsersPath() + '/' + user, METHOD_VALUE, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (isShv3()) {
			m_editUser = shv3AclUserFromRpcValue(result);
		}
		else {
			m_editUser = shv::iotqt::acl::AclUser::fromRpcValue(result);
		}
		setStatusText({});
		callback(true);
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to get user roles.") + " " + error);
		callback(false);
	});
}

void DlgSettings::callSaveUser(std::function<void(bool)> callback)
{
	auto user = ui->leUserName->text().toStdString();
	auto password = ui->leUserPassword->text().toStdString();

	m_editUser.roles = {};
	for (const auto &role : stringListFromLineEdit(ui->leUserRoles)) {
		m_editUser.roles.push_back(role.toStdString());
	}

	if (!password.empty()) {
		// user wants to change password
		m_editUser.password.format = shv::iotqt::acl::AclPassword::Format::Sha1;
		m_editUser.password.password = sha1_hex(password);
	}

	shv::chainpack::RpcValue user_rv;
	if (isShv3()) {
		user_rv = shv3AclUserToRpcValue(m_editUser);
	}
	else {
		user_rv = m_editUser.toRpcValue();
	}

	shv::chainpack::RpcValue::List params{user, user_rv};
	callShvMethod(aclAccessUsersPath(), METHOD_SET_VALUE, params, [callback](const shv::chainpack::RpcValue &) {
		callback(true);
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to save user settings.") + " " + error);
		callback(false);
	});
}

void DlgSettings::checkExistingUser(std::function<void(bool, bool)> callback)
{
	callShvMethod(aclAccessUsersPath(), shv::chainpack::Rpc::METH_LS, {}, [this, callback](const shv::chainpack::RpcValue &result) {
		if (result.isList()) {
			auto user = ui->leUserName->text().toStdString();
			for (const auto &item : result.asList()) {
				if (item.asString() == user) {
					callback(true, true);
					return;
				}
			}
			callback(true, false);
		}
		else {
			setStatusText(tr("Failed to check user name. Bad server response format."));
			callback(false, false);
		}
	}, [this, callback](const QString &error) {
		setStatusText(tr("Failed to check user name.") + " " + error);
		callback(false, false);
	});
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

void DlgSettings::reloadRoles(const QString &role_to_select)
{
	clearRoles();
	setRoleControlsEnabled(false);
	setStatusText(tr("Reloading roles..."));
	loadRoles([this, role_to_select](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setRoleControlsEnabled(true);
			setCurrentRow(ui->twRoles, role_to_select);
			setStatusText({});
		}
	});
}

void DlgSettings::onAddRoleClicked()
{
	showRoleEdit();
	ui->roleGroupBox->setTitle(tr("New role"));
	ui->leRoleName->setReadOnly(false);
	ui->leRoleName->setFocus();
}

void DlgSettings::onEditRoleClicked()
{
	QString role = currentRow(ui->twRoles);
	if (role.isEmpty()){
		setStatusText(tr("Select role in the table."));
		return;
	}
	setRoleControlsEnabled(false);
	setStatusText(tr("Getting role details..."));
	callGetRole([this, role](bool success, const QStringList &roles, const shv::chainpack::RpcValue &profile, const std::optional<int> &weight, const shv::chainpack::RpcValue &access) {
		const bool connected = m_rpcConnection->isBrokerConnected();
		setRoleControlsEnabled(connected);
		if (!success || !connected) {
			return;
		}

		showRoleEdit();
		ui->roleGroupBox->setTitle(tr("Edit role"));
		ui->leRoleName->setReadOnly(true);
		ui->leRoleName->setText(role);
		setStringListToLineEdit(ui->leRoles, roles);
		if (profile.isMap()) {
			ui->leProfile->setText(QString::fromStdString(profile.toCpon()));
		}
		else {
			ui->leProfile->setText({});
		}
		if (weight) {
			ui->sbWeight->setValue(weight.value());
			ui->lblWeight->show();
			ui->sbWeight->show();
		}
		else {
			ui->lblWeight->hide();
			ui->sbWeight->hide();
		}
		m_accessModel->setRules(access);
		ui->tvAccessRules->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
		setStatusText({});
	});
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
			reloadRoles({});
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

void DlgSettings::reloadMounts(const QString &mount_point_to_select)
{
	clearMounts();
	setMountControlsEnabled(false);
	setStatusText(tr("Reloading mount points..."));
	loadMounts([this, mount_point_to_select](bool success){
		if (success && m_rpcConnection->isBrokerConnected()) {
			setMountControlsEnabled(true);
			setCurrentRow(ui->twMounts, mount_point_to_select);
			setStatusText({});
		}
	});
}

void DlgSettings::onAddMountClicked()
{
	showMountEdit();
	ui->editMountWidget->setTitle(tr("New mount point"));
	ui->leMountDeviceId->setReadOnly(false);
	ui->leMountDeviceId->setFocus();
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
			reloadMounts({});
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
	setMountControlsEnabled(false);
	callGetMount([this, mount](bool success, const shv::iotqt::acl::AclMountDef &mount_def) {
		const bool connected = m_rpcConnection->isBrokerConnected();
		setMountControlsEnabled(connected);
		if (!success || !connected) {
			return;
		}

		showMountEdit();
		ui->editMountWidget->setTitle(tr("Edit mount point"));
		ui->leMountDeviceId->setReadOnly(true);
		ui->leMountDeviceId->setText(mount);
		ui->leMountPoint->setText(QString::fromStdString(mount_def.mountPoint));
		ui->leMountDescription->setText(QString::fromStdString(mount_def.description));
	});
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
