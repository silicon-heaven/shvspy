#include "dlgmountseditor.h"
#include "ui_dlgmountseditor.h"

#include "dlgaddeditmount.h"

#include <shv/core/assert.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

static const std::string METHOD_VALUE = "value";
static const std::string METHOD_SET_VALUE = "setValue";

DlgMountsEditor::DlgMountsEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection) :
	QDialog(parent),
	ui(new Ui::DlgMountsEditor)
{
	ui->setupUi(this);

	SHV_ASSERT_EX(rpc_connection != nullptr, "RPC connection is NULL");

	m_rpcConnection = rpc_connection;

	static constexpr double ROW_HEIGHT_RATIO = 1.3;
	static QStringList INFO_HEADER_NAMES { tr("Device ID"), tr("Mount point"), tr("Description") };

	m_dataModel = new QStandardItemModel(this);
	m_dataModel->setColumnCount(static_cast<int>(INFO_HEADER_NAMES.count()));
	m_dataModel->setHorizontalHeaderLabels(INFO_HEADER_NAMES);

	m_modelProxy = new QSortFilterProxyModel(this);
	m_modelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_modelProxy->setSourceModel(m_dataModel);
	m_modelProxy->setFilterKeyColumn(-1);
	ui->twMounts->setModel(m_modelProxy);

	ui->twMounts->horizontalHeader()->setStretchLastSection(true);
	ui->twMounts->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twMounts->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twMounts->verticalHeader()->setVisible(false);
	ui->twMounts->setSelectionBehavior(QAbstractItemView::SelectRows);

	connect(ui->pbAddMount, &QPushButton::clicked, this, &DlgMountsEditor::onAddMountClicked);
	connect(ui->pbDeleteMount, &QPushButton::clicked, this, &DlgMountsEditor::onDeleteMountClicked);
	connect(ui->pbEditMount, &QPushButton::clicked, this, &DlgMountsEditor::onEditMountClicked);
	connect(ui->twMounts, &QTableView::doubleClicked, this, &DlgMountsEditor::onTableMountDoubleClicked);
	connect(ui->leFilter, &QLineEdit::textChanged, m_modelProxy, &QSortFilterProxyModel::setFilterFixedString);

	setStatusText(QString());
}

DlgMountsEditor::~DlgMountsEditor()
{
	delete ui;
}

void DlgMountsEditor::init(const std::string &acl_node_path)
{
	m_aclEtcNodePath = acl_node_path;
	listMounts();
}

std::string DlgMountsEditor::aclEtcMountsNodePath()
{
	return m_aclEtcNodePath + "/mounts";
}

QString DlgMountsEditor::selectedMount()
{
	QModelIndex current_index = ui->twMounts->currentIndex();
	if (current_index.isValid()) {
		return m_modelProxy->index(current_index.row(), 0).data().toString();
	}

	return QString();
}

void DlgMountsEditor::onAddMountClicked()
{
	auto dlg = new DlgAddEditMount(this, m_rpcConnection, m_aclEtcNodePath, DlgAddEditMount::DialogType::Add);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listMounts();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgMountsEditor::onDeleteMountClicked()
{
	std::string mount = selectedMount().toStdString();

	if (mount.empty()){
		setStatusText(tr("Select device id in the table above."));
		return;
	}

	setStatusText(QString());

	if (QMessageBox::question(this, tr("Delete mount"), tr("Do you really want to delete mount definition") + " " + QString::fromStdString(mount) + "?") == QMessageBox::Yes){
		int rqid = m_rpcConnection->nextRequestId();
		auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

		cb->start(this, [this, mount](const shv::chainpack::RpcResponse &response) {
			if(response.isValid()){
				if(response.isError()) {
					setStatusText(tr("Failed to delete mount definition.") + " " + QString::fromStdString(response.error().toString()));
				}
				else {
					listMounts();
				}
			}
			else{
				setStatusText(tr("Request timeout expired"));
			}
		});

		shv::chainpack::RpcValue::List params{mount, {}};
		m_rpcConnection->callShvMethod(rqid, aclEtcMountsNodePath(), METHOD_SET_VALUE, params);
	}
}

void DlgMountsEditor::onEditMountClicked()
{
	QString mount = selectedMount();

	if (mount.isEmpty()){
		setStatusText(tr("Select device id in the table above."));
		return;
	}

	setStatusText(QString());

	auto dlg = new DlgAddEditMount(this, m_rpcConnection, m_aclEtcNodePath, DlgAddEditMount::DialogType::Edit);
	dlg->init(mount);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listMounts();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgMountsEditor::onTableMountDoubleClicked(QModelIndex ix)
{
	Q_UNUSED(ix);
	onEditMountClicked();
}

void DlgMountsEditor::onRpcCallsFinished()
{
	ui->pbAddMount->setEnabled(true);
	ui->pbEditMount->setEnabled(true);
	ui->pbDeleteMount->setEnabled(true);
	ui->leFilter->setEnabled(true);
	setStatusText(QString());

	m_dataModel->setRowCount(static_cast<int>(m_mountPoints.count()));
	int i = 0;
	for (const MountPointInfo &info : std::as_const(m_mountPoints)) {
		auto *id_item = new QStandardItem(info.id);
		id_item->setFlags(id_item->flags() & ~Qt::ItemIsEditable);
		m_dataModel->setItem(i, 0, id_item);

		auto *mountpoint_item = new QStandardItem(info.mountPoint);
		mountpoint_item->setFlags(mountpoint_item->flags() & ~Qt::ItemIsEditable);
		m_dataModel->setItem(i, 1, mountpoint_item);

		auto *description_item = new QStandardItem(info.description);
		description_item->setFlags(description_item->flags() & ~Qt::ItemIsEditable);
		m_dataModel->setItem(i, 2, description_item);

		++i;
	}

	if (!m_lastCurrentId.isEmpty()) {
		for (int i = 0; i < m_modelProxy->rowCount(); ++i) {
			if (m_modelProxy->index(i, 0).data().toString() == m_lastCurrentId) {
				ui->twMounts->setCurrentIndex(m_modelProxy->index(i, 0));
			}
		}
	}
	m_mountPoints.clear();
}

void DlgMountsEditor::listMounts()
{
	if (m_rpcConnection == nullptr)
		return;

	m_lastCurrentId = selectedMount();

	m_dataModel->removeRows(0, m_dataModel->rowCount());
	m_mountPoints.clear();

	ui->pbAddMount->setEnabled(false);
	ui->pbEditMount->setEnabled(false);
	ui->pbDeleteMount->setEnabled(false);
	ui->leFilter->setEnabled(false);
	setStatusText(tr("Loading..."));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isSuccess()){
			if (response.result().isList()){
				const auto result = response.result();
				const auto &res = result.asList();

				for (const auto & re : res){
					QString id = QString::fromStdString(re.asString());
					m_mountPoints[id].id = id;
					getMountPointDefinition(id);
				}
			}
		}
		else{
			setStatusText(tr("Failed to load mount definition.") + " " + QString::fromStdString(response.error().toString()));
		}
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcMountsNodePath(), shv::chainpack::Rpc::METH_LS);
}

void DlgMountsEditor::getMountPointDefinition(const QString &id)
{
	if (m_rpcConnection == nullptr)
		return;

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this, id](const shv::chainpack::RpcResponse &response) {
		if (response.isSuccess()) {
			m_mountPoints[id].status = Ok;
			if (response.result().isMap()){
				m_mountPoints[id].mountPoint = QString::fromStdString(response.result().at("mountPoint").asString());
				m_mountPoints[id].description = QString::fromStdString(response.result().at("description").asString());
			}
		}
		else {
			m_mountPoints[id].status = Error;
			setStatusText(tr("Failed to load mountpoint definition for ID: ") + id
						  + " - " + QString::fromStdString(response.errorString()));
		}
		checkRpcCallsFinished();
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcMountsNodePath() + "/" + id.toStdString(), METHOD_VALUE);
}

void DlgMountsEditor::checkRpcCallsFinished()
{
	for (const auto &mount_point : std::as_const(m_mountPoints)) {
		if (mount_point.status != Ok) {
			return;
		}
	}
	onRpcCallsFinished();
}

void DlgMountsEditor::setStatusText(const QString &txt)
{
	if(txt.isEmpty()) {
		ui->lblStatus->hide();
	}
	else {
		ui->lblStatus->show();
		ui->lblStatus->setText(txt);
	}
}

