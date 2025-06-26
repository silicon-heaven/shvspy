#include "dlgaddeditrole.h"
#include "ui_dlgaddeditrole.h"
#include "accessmodel/accessmodel.h"
#include "accessmodel/accessitemdelegate.h"

#include "shv/coreqt/log.h"
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QMessageBox>

namespace cp = shv::chainpack;

using namespace std;

static const std::string VALUE_METHOD = "value";
static const std::string SET_VALUE_METHOD = "setValue";

DlgAddEditRole::DlgAddEditRole(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, const QString &role_name, DlgAddEditRole::DialogType dt)
	: QDialog(parent)
	, ui(new Ui::DlgAddEditRole)
	, m_aclEtcNodePath(acl_etc_node_path)
	, m_accessModel(new AccessModel(this))
{
	ui->setupUi(this);
	m_dialogType = dt;
	bool edit_mode = (m_dialogType == DialogType::Edit);

	ui->leRoleName->setReadOnly(edit_mode);
	ui->groupBox->setTitle(edit_mode ? tr("Edit role") : tr("New role"));
	setWindowTitle(edit_mode ? tr("Edit role dialog") : tr("New role dialog"));

	ui->tvAccessRules->setModel(m_accessModel);
	ui->tvAccessRules->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
	ui->tvAccessRules->verticalHeader()->setDefaultSectionSize(static_cast<int>(fontMetrics().height() * 1.3));
	ui->tvAccessRules->setColumnWidth(AccessModel::Columns::ColPath, static_cast<int>(frameGeometry().width() * 0.6));

	auto *del = new AccessItemDelegate(ui->tvAccessRules);
	ui->tvAccessRules->setItemDelegate(del);

	connect(ui->tbAddRow, &QToolButton::clicked, this, &DlgAddEditRole::onAddRowClicked);
	connect(ui->tbDeleteRow, &QToolButton::clicked, this, &DlgAddEditRole::onDeleteRowClicked);
	connect(ui->tbMoveRowUp, &QToolButton::clicked, this, &DlgAddEditRole::onMoveRowUpClicked);
	connect(ui->tbMoveRowDown, &QToolButton::clicked, this, &DlgAddEditRole::onMoveRowDownClicked);

	m_rpcConnection = rpc_connection;

	setStatusText((m_rpcConnection == nullptr) ? tr("Connection to shv does not exist.") : QString());

	if (dt == DialogType::Edit) {
		Q_ASSERT(!role_name.isEmpty());
		loadRole(role_name);
	}
}

DlgAddEditRole::~DlgAddEditRole()
{
	delete ui;
}

DlgAddEditRole::DialogType DlgAddEditRole::dialogType()
{
	return m_dialogType;
}

void DlgAddEditRole::loadRole(const QString &role_name)
{
	ui->leRoleName->setText(role_name);
	callGetRoleSettings();
	callGetAccessRulesForRole();
}

void DlgAddEditRole::accept()
{
	if (roleName().isEmpty()){
		QMessageBox::critical(this, tr("Invalid data"), tr("Role name is empty."));
		return;
	}
	if (!m_accessModel->isRulesValid()){
		QMessageBox::critical(this, tr("Invalid data"), 	tr("Access rules are invalid."));
		return;
	}

	setStatusText(QString());

	if (dialogType() == DialogType::Add){
		setStatusText(tr("Checking role existence"));
		checkExistingRole([this](bool success, bool is_duplicate) {
			if (success) {
				if (is_duplicate) {
					setStatusText(tr("Cannot add role, role name is duplicate!"));
					return;
				}
				setStatusText(tr("Adding new role ..."));
				callSetRoleSettings();
			}
		});
	}
	else if (dialogType() == DialogType::Edit){
		setStatusText(tr("Updating role ..."));
		callSetRoleSettings();
	}
}

void DlgAddEditRole::checkExistingRole(std::function<void(bool, bool)> callback)
{
	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this, callback](const shv::chainpack::RpcResponse &response) {
		if (response.isSuccess()) {
			if (!response.result().isList()) {
				setStatusText(tr("Failed to check user name. Bad server response format."));
				callback(false, false);
			}
			else {
				std::string role_name = roleName().toStdString();
				const auto res = response.result();
				for (const auto &item : res.asList()) {
					if (item.asString() == role_name) {
						callback(true, true);
						return;
					}
				}
				callback(true, false);
			}
		}
		else {
			setStatusText(tr("Failed to check user name.") + " " + QString::fromStdString(response.error().toString()));
			callback(false, false);
		}
	});

	m_rpcConnection->callShvMethod(rqid, rolesShvPath(), shv::chainpack::Rpc::METH_LS);
}

void DlgAddEditRole::callSetRoleSettings()
{
	if (m_rpcConnection == nullptr)
		return;

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Failed:") + QString::fromStdString(response.error().toString()));
			}
			else{
				callSetAccessRulesForRole();
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	m_role.roles = roles();
	m_role.profile = profile();

	auto role_rpc = m_role.toRpcValue();
	if (ui->sbWeight->isVisible()) {
		role_rpc.set("weight", ui->sbWeight->value());
	}

	shv::chainpack::RpcValue::List params{roleName().toStdString(), role_rpc};
	m_rpcConnection->callShvMethod(rqid, rolesShvPath(), SET_VALUE_METHOD, params);
}

void DlgAddEditRole::callGetRoleSettings()
{
	if(m_rpcConnection == nullptr){
		return;
	}

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if (response.isError()) {
				setStatusText(tr("Failed to call method %1.").arg(QString::fromStdString(roleShvPath() + ':' + VALUE_METHOD)) + QString::fromStdString(response.error().toString()));
			}
			else{
				m_role = shv::iotqt::acl::AclRole::fromRpcValue(response.result()).value_or(shv::iotqt::acl::AclRole());
				setRoles(m_role.roles);
				setProfile(m_role.profile);
				// libshv no longer supports weight, so we need to handle it manually.
				if (auto v = response.result().asMap().value("weight"); v.isInt()) {
					setWeight(v.toInt());
					ui->lblWeight->show();
					ui->sbWeight->show();
				} else {
					ui->lblWeight->hide();
					ui->sbWeight->hide();
				}
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	m_rpcConnection->callShvMethod(rqid, roleShvPath(), VALUE_METHOD);
}

void DlgAddEditRole::callGetAccessRulesForRole()
{
	if (m_rpcConnection == nullptr)
		return;

	setStatusText(tr("Getting access rules ..."));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Error: %1").arg(QString::fromStdString(response.error().toString())));
			}
			else {
				m_accessModel->setRules(response.result());
				setStatusText(QString());
				return;
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
		m_accessModel->setRules(shv::chainpack::RpcValue());
	});

	m_rpcConnection->callShvMethod(rqid, roleAccessShvPath(), VALUE_METHOD);
}

void DlgAddEditRole::callSetAccessRulesForRole()
{
	if (m_rpcConnection == nullptr) {
		return;
	}

	setStatusText(tr("Updating access rules ..."));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Error: %1").arg(QString::fromStdString(response.error().toString())));
			}
			else{
				QDialog::accept();
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	shv::chainpack::RpcValue::List params{roleName().toStdString(), m_accessModel->rules()};
	m_rpcConnection->callShvMethod(rqid, accessShvPath(), SET_VALUE_METHOD, params);
}

std::vector<std::string> DlgAddEditRole::roles() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		auto skip_empty_parts = QString::SkipEmptyParts;
#else
		auto skip_empty_parts = Qt::SkipEmptyParts;
#endif
	QStringList lst = ui->leRoles->text().split(",", skip_empty_parts);

	std::vector<std::string> rlst;
	for (int i = 0; i < lst.count(); i++){
		rlst.push_back(lst.at(i).trimmed().toStdString());
	}

	return rlst;
}

void DlgAddEditRole::setRoles(const std::vector<std::string> &roles)
{
	QString rls;
	if(!roles.empty())
		rls = std::accumulate(std::next(roles.begin()), roles.end(), QString::fromStdString(roles[0]),
					   [](const QString &s1, const std::string &s2) -> QString { return s1 + ',' + QString::fromStdString(s2); });
	ui->leRoles->setText(rls);
}

QString DlgAddEditRole::roleName() const
{
	return ui->leRoleName->text();
}

int DlgAddEditRole::weight() const
{
	return ui->sbWeight->value();
}

void DlgAddEditRole::setWeight(int weight)
{
	ui->sbWeight->setValue(weight);
}

shv::chainpack::RpcValue DlgAddEditRole::profile() const
{
	string s = ui->leProfile->text().trimmed().toStdString();
	if(s.empty())
		return cp::RpcValue();
	string err;
	auto rv = cp::RpcValue::fromCpon(s, &err);
	if(!err.empty())
		shvWarning() << roleName() << "invalid profile definition:" << s;
	return rv;
}

void DlgAddEditRole::setProfile(const shv::chainpack::RpcValue &p)
{
	if(p.isMap())
		ui->leProfile->setText(QString::fromStdString(p.toCpon()));
	else
		ui->leProfile->setText(QString());
}

std::string DlgAddEditRole::rolesShvPath()
{
	return aclShvPath() + "/roles";
}

std::string DlgAddEditRole::aclShvPath()
{
	return m_aclEtcNodePath;
}

std::string DlgAddEditRole::roleShvPath()
{
	return rolesShvPath() + '/' + roleName().toStdString();
}

string DlgAddEditRole::accessShvPath()
{
	return aclShvPath() + "/access";
}

string DlgAddEditRole::roleAccessShvPath()
{
	return accessShvPath() + "/" + roleName().toStdString();
}

void DlgAddEditRole::onAddRowClicked()
{
	m_accessModel->addRule();
}

void DlgAddEditRole::onDeleteRowClicked()
{
	m_accessModel->deleteRule(ui->tvAccessRules->currentIndex().row());
}

void DlgAddEditRole::onMoveRowUpClicked()
{
	m_accessModel->moveRuleUp(ui->tvAccessRules->currentIndex().row());
}

void DlgAddEditRole::onMoveRowDownClicked()
{
	m_accessModel->moveRuleDown(ui->tvAccessRules->currentIndex().row());
}

void DlgAddEditRole::setStatusText(const QString &txt)
{
	if(txt.isEmpty()) {
		ui->lblStatus->hide();
	}
	else {
		ui->lblStatus->show();
		ui->lblStatus->setText(txt);
	}
}

