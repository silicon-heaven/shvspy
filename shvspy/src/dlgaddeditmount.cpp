#include "dlgaddeditmount.h"
#include "ui_dlgaddeditmount.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/core/log.h>

#include <QMessageBox>

static const std::string VALUE_METHOD = "value";
static const std::string SET_VALUE_METHOD = "setValue";

DlgAddEditMount::DlgAddEditMount(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, DlgAddEditMount::DialogType dt) :
	QDialog(parent),
	ui(new Ui::DlgAddEditMount),
	m_aclEtcNodePath(acl_etc_node_path)
{
	ui->setupUi(this);
	m_dialogType = dt;
	bool edit_mode = (m_dialogType == DialogType::Edit);

	ui->leDeviceId->setReadOnly(edit_mode);
	ui->groupBox->setTitle(edit_mode ? tr("Edit mount") : tr("New mount"));
	setWindowTitle(edit_mode ? tr("Edit mount dialog") : tr("New mount dialog"));

	m_rpcConnection = rpc_connection;

	setStatusText((m_rpcConnection == nullptr)? tr("Connection to shv does not exist."): QString());
}

DlgAddEditMount::~DlgAddEditMount()
{
	delete ui;
}

DlgAddEditMount::DialogType DlgAddEditMount::dialogType()
{
	return m_dialogType;
}

void DlgAddEditMount::init(const QString &device_id)
{
	ui->leDeviceId->setText(device_id);
	callGetMountSettings();
}

void DlgAddEditMount::accept()
{
	if (dialogType() == DialogType::Add){
		setStatusText(tr("Checking mount ID existence"));
		checkExistingMountId([this](bool success, bool is_duplicate) {
			if (success) {
				if (is_duplicate) {
					setStatusText(tr("Cannot add mount point, mount ID is duplicate!"));
					return;
				}
				setStatusText(tr("Adding new mount ..."));
				callSetMountSettings();
			}
		});
	}
	else if (dialogType() == DialogType::Edit){
		setStatusText(tr("Updating mount definition ..."));
		callSetMountSettings();
	}
}

void DlgAddEditMount::checkExistingMountId(std::function<void(bool, bool)> callback)
{
	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this, callback](const shv::chainpack::RpcResponse &response) {
		if (response.isSuccess()) {
			if (!response.result().isList()) {
				setStatusText(tr("Failed to check mount ID. Bad server response format."));
				callback(false, false);
			}
			else {
				std::string mount_id = ui->leDeviceId->text().toStdString();
				const auto res = response.result();
				for (const auto &item : res.asList()) {
					if (item.asString() == mount_id) {
						callback(true, true);
						return;
					}
				}
				callback(true, false);
			}
		}
		else {
			setStatusText(tr("Failed to check mount ID.") + " " + QString::fromStdString(response.error().toString()));
			callback(false, false);
		}
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcMountNodePath(), shv::chainpack::Rpc::METH_LS);
}

void DlgAddEditMount::callSetMountSettings()
{
	if (ui->leDeviceId->text().trimmed().isEmpty()){
		setStatusText(tr("Error: device id is empty."));
		return;
	}
	if (ui->leMountPoint->text().trimmed().isEmpty()) {
		setStatusText(tr("Error: mount point is empty."));
		return;
	}

	if (m_rpcConnection == nullptr)
		return;

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Failed: ") + QString::fromStdString(response.error().toString()));
			}
			else{
				QDialog::accept();
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	m_mount.mountPoint = ui->leMountPoint->text().toStdString();
	m_mount.description = ui->leDescription->text().toStdString();

	shv::chainpack::RpcValue::List params{ui->leDeviceId->text().toStdString(), m_mount.toRpcValue()};
	m_rpcConnection->callShvMethod(rqid, aclEtcMountNodePath(), SET_VALUE_METHOD, params);
}

void DlgAddEditMount::callGetMountSettings()
{
	if(m_rpcConnection == nullptr){
		return;
	}

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if (response.isError()) {
				setStatusText(tr("Failed to call method ls.") + QString::fromStdString(response.error().toString()));
			}
			else{
				m_mount = shv::iotqt::acl::AclMountDef::fromRpcValue(response.result());
				ui->leMountPoint->setText(QString::fromStdString(m_mount.mountPoint));
				ui->leDescription->setText(QString::fromStdString(m_mount.description));
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	m_rpcConnection->callShvMethod(rqid, mountShvPath(), VALUE_METHOD);
}

std::string DlgAddEditMount::aclEtcMountNodePath()
{
	return m_aclEtcNodePath + "/mounts";
}

std::string DlgAddEditMount::mountShvPath()
{
	return aclEtcMountNodePath() + '/' + ui->leDeviceId->text().toStdString();
}

void DlgAddEditMount::setStatusText(const QString &txt)
{
	if(txt.isEmpty()) {
		ui->lblStatus->hide();
	}
	else {
		ui->lblStatus->show();
		ui->lblStatus->setText(txt);
	}
}

