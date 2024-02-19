#include "dlgaddedituser.h"
#include "ui_dlgaddedituser.h"

#include "dlgselectroles.h"

#include <shv/iotqt/acl/aclrole.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QCryptographicHash>
#include <QMessageBox>

static const std::string VALUE_METHOD = "value";
static const std::string SET_VALUE_METHOD = "setValue";

DlgAddEditUser::DlgAddEditUser(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, DlgAddEditUser::DialogType dt)
	: QDialog(parent)
	, ui(new Ui::DlgAddEditUser)
	, m_aclEtcNodePath(acl_etc_node_path)
{
	ui->setupUi(this);
	m_dialogType = dt;
	bool edit_mode = (m_dialogType == DialogType::Edit);

	ui->leUserName->setReadOnly(edit_mode);
	ui->groupBox->setTitle(edit_mode ? tr("Edit user") : tr("New user"));
	setWindowTitle(edit_mode ? tr("Edit user dialog") : tr("New user dialog"));
	ui->lblPassword->setText(edit_mode ? tr("New password") : tr("Password"));

	m_rpcConnection = rpc_connection;

	if(m_rpcConnection == nullptr){
		ui->lblStatus->setText(tr("Connection to shv does not exist."));
	}

	connect(ui->tbShowPassword, &QToolButton::clicked, this, &DlgAddEditUser::onShowPasswordClicked);
	connect(ui->pbSelectRoles, &QAbstractButton::clicked, this, &DlgAddEditUser::onSelectRolesClicked);
}

DlgAddEditUser::~DlgAddEditUser()
{
	delete ui;
}

static std::string sha1_hex(const std::string &s)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Sha1);
#if QT_VERSION_MAJOR >= 6 && QT_VERSION_MINOR >= 3
	hash.addData(QByteArrayView(s.data(), s.length()));
#else
	hash.addData(s.data(), s.length());
#endif
	return std::string(hash.result().toHex().constData());
}

DlgAddEditUser::DialogType DlgAddEditUser::dialogType()
{
	return m_dialogType;
}

std::string DlgAddEditUser::user()
{
	return ui->leUserName->text().toStdString();
}

void DlgAddEditUser::setUser(const QString &user)
{
	ui->leUserName->setText(user);
	callGetUserSettings();
}

QString DlgAddEditUser::password()
{
	return ui->lePassword->text();
}

void DlgAddEditUser::accept()
{
	if (dialogType() == DialogType::Add){
		if ((!user().empty()) && (!password().isEmpty())){
			ui->lblStatus->setText(tr("Checking user name existence"));
			checkExistingUser([this](bool success, bool is_duplicate) {
				if (success) {
					if (is_duplicate) {
						ui->lblStatus->setText(tr("Cannot add user, user name is duplicate!"));
						return;
					}
					ui->lblStatus->setText(tr("Adding new user"));

					if (ui->chbCreateRole->isChecked()){
						callCreateRole(user(), [this](){
							callSetUserSettings();
						});
					}
					else{
						callSetUserSettings();
					}
				}
			});
		}
		else {
			ui->lblStatus->setText(tr("User name or password is empty."));
		}
	}
	else if (dialogType() == DialogType::Edit){
		ui->lblStatus->setText(tr("Updating user ...") + QString::fromStdString(aclEtcUsersNodePath()));

		if (ui->chbCreateRole->isChecked()){
			callCreateRole(user(), [this]() {
				callSetUserSettings();
			});
		}
		else{
			callSetUserSettings();
		}
	}
}

void DlgAddEditUser::checkExistingUser(std::function<void(bool, bool)> callback)
{
	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this, callback](const shv::chainpack::RpcResponse &response) {
		if (response.isSuccess()) {
			if (!response.result().isList()) {
				ui->lblStatus->setText(tr("Failed to check user name. Bad server response format."));
				callback(false, false);
			}
			else {
				std::string user_name = user();
				const auto res = response.result();
				for (const auto &item : res.asList()) {
					if (item.asString() == user_name) {
						callback(true, true);
						return;
					}
				}
				callback(true, false);
			}
		}
		else {
			ui->lblStatus->setText(tr("Failed to check user name.") + " " + QString::fromStdString(response.error().toString()));
			callback(false, false);
		}
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcUsersNodePath(), shv::chainpack::Rpc::METH_LS);
}

void DlgAddEditUser::onShowPasswordClicked()
{
	bool password_mode = (ui->lePassword->echoMode() == QLineEdit::EchoMode::Password);
	ui->lePassword->setEchoMode((password_mode) ? QLineEdit::EchoMode::Normal : QLineEdit::EchoMode::Password);
	ui->tbShowPassword->setIcon((password_mode) ? QIcon(":/shvspy/images/hide.svg") : QIcon(":/shvspy/images/show.svg"));
}

void DlgAddEditUser::onSelectRolesClicked()
{
	if (ui->chbCreateRole->isChecked() && !ui->leUserName->text().isEmpty()){

		if (QMessageBox::question(this, tr("Confirm create role"),
								  tr("You are requesting create new role. So you can select roles properly, "
									 "new role must be created now. It will not be deleted if you cancel this dialog. "
									 "Do you want to continue?")) == QMessageBox::StandardButton::Yes){

			callCreateRole(user(), [this](){
				execSelectRolesDialog();
			});
		}
	}
	else {
		execSelectRolesDialog();
	}
}

void DlgAddEditUser::execSelectRolesDialog()
{
	auto dlg = new DlgSelectRoles(this);
	dlg->init(m_rpcConnection, m_aclEtcNodePath, roles());
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			setRoles(dlg->selectedRoles());
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgAddEditUser::callCreateRole(const std::string &role_name, std::function<void()> callback)
{
	if (m_rpcConnection == nullptr)
		return;

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this, callback](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if(response.isError()) {
				ui->lblStatus->setText(tr("Failed to add role.") + QString::fromStdString(response.error().toString()));
			}
			else{
				callback();
			}
		}
		else{
			ui->lblStatus->setText(tr("Request timeout expired"));
		}
	});

	shv::iotqt::acl::AclRole role;
	auto role_rpc = role.toRpcValue();
	role_rpc.set("weight", 0);

	shv::chainpack::RpcValue::List params{role_name, role_rpc};
	m_rpcConnection->callShvMethod(rqid, aclEtcRolesNodePath(), SET_VALUE_METHOD, params);
}

void DlgAddEditUser::callGetUserSettings()
{
	if (m_rpcConnection == nullptr)
		return;

	ui->lblStatus->setText(tr("Getting settings ..."));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				ui->lblStatus->setText(QString::fromStdString(response.error().toString()));
			}
			else{
				m_user = shv::iotqt::acl::AclUser::fromRpcValue(response.result());
				setRoles(m_user.roles);
				ui->lblStatus->setText("");
			}
		}
		else{
			ui->lblStatus->setText(tr("Request timeout expired"));
		}
	});

	m_rpcConnection->callShvMethod(rqid, userShvPath(), VALUE_METHOD);
}

void DlgAddEditUser::callSetUserSettings()
{
	if (m_rpcConnection == nullptr)
		return;

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if(response.isError()) {
				ui->lblStatus->setText(QString::fromStdString(response.error().toString()));
			}
			else{
				QDialog::accept();
			}
		}
		else{
			ui->lblStatus->setText(tr("Request timeout expired"));
		}
	});

	shv::chainpack::RpcValue::Map user_settings;

	m_user.roles = roles();

	if (!password().isEmpty()){
		m_user.password.format = shv::iotqt::acl::AclPassword::Format::Sha1;
		m_user.password.password = sha1_hex(password().toStdString());
	}

	shv::chainpack::RpcValue::List params{user(), m_user.toRpcValue()};
	m_rpcConnection->callShvMethod(rqid, aclEtcUsersNodePath(), SET_VALUE_METHOD, params);
}

std::string DlgAddEditUser::aclEtcUsersNodePath()
{
	return m_aclEtcNodePath + "/users";
}

std::string DlgAddEditUser::aclEtcRolesNodePath()
{
	return m_aclEtcNodePath + "/roles";
}

std::string DlgAddEditUser::userShvPath()
{
	return aclEtcUsersNodePath() + '/' + user();
}

std::vector<std::string> DlgAddEditUser::roles()
{
	std::vector<std::string> roles;
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		auto skip_empty_parts = QString::SkipEmptyParts;
#else
		auto skip_empty_parts = Qt::SkipEmptyParts;
#endif
	QStringList lst = ui->leRoles->text().split(",", skip_empty_parts);

	for (int i = 0; i < lst.count(); i++){
		roles.push_back(lst.at(i).trimmed().toStdString());
	}

	if (ui->chbCreateRole->isChecked())
		 roles.push_back(user());

	return roles;
}

void DlgAddEditUser::setRoles(const std::vector<std::string> &roles)
{
	QString rls;
	if(!roles.empty())
		rls = std::accumulate(std::next(roles.begin()), roles.end(), QString::fromStdString(roles[0]),
					   [](const QString &s1, const std::string &s2) -> QString { return s1 + ',' + QString::fromStdString(s2); });
	ui->leRoles->setText(rls);
}

void DlgAddEditUser::setRoles(const shv::chainpack::RpcValue::List &roles)
{
	QStringList roles_list;

	for (const auto& role : roles){
		if (role.isString()){
			roles_list.append(QString::fromStdString(role.asString()));
		}
	}

	ui->leRoles->setText(roles_list.join(","));
}
