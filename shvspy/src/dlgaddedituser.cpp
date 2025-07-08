#include "dlgaddedituser.h"
#include "ui_dlgaddedituser.h"

#include "dlgselectroles.h"

#include <shv/iotqt/acl/aclrole.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/clientconnection.h>

#include <QCryptographicHash>
#include <QMessageBox>

using namespace shv::chainpack;

static const std::string VALUE_METHOD = "value";
static const std::string SET_VALUE_METHOD = "setValue";

DlgAddEditUser::DlgAddEditUser(
		shv::iotqt::rpc::ClientConnection *rpc_connection,
		const std::string &acl_etc_node_path,
		const QString &user,
		QWidget *parent
	)
	: QDialog(parent)
	, ui(new Ui::DlgAddEditUser)
	, m_rpcConnection(rpc_connection)
	, m_aclEtcNodePath(acl_etc_node_path)
{
	Q_ASSERT(m_rpcConnection);

	ui->setupUi(this);

	m_shvApiVersion = m_rpcConnection->shvApiVersion();

	m_dialogType = user.isEmpty()? DialogType::Add: DialogType::Edit;
	bool edit_mode = (m_dialogType == DialogType::Edit);

	ui->leUserName->setReadOnly(edit_mode);
	ui->groupBox->setTitle(edit_mode ? tr("Edit user") : tr("New user"));
	setWindowTitle(edit_mode ? tr("Edit user dialog") : tr("New user dialog"));
	ui->lblPassword->setText(edit_mode ? tr("New password") : tr("Password"));

	m_rpcConnection = rpc_connection;

	connect(ui->tbShowPassword, &QToolButton::clicked, this, &DlgAddEditUser::onShowPasswordClicked);
	connect(ui->pbSelectRoles, &QAbstractButton::clicked, this, &DlgAddEditUser::onSelectRolesClicked);

	if (edit_mode) {
		ui->leUserName->setText(user);
		callGetUserSettings();
	}
}

DlgAddEditUser::~DlgAddEditUser()
{
	delete ui;
}

namespace {
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

std::string DlgAddEditUser::user()
{
	return ui->leUserName->text().toStdString();
}

QString DlgAddEditUser::password()
{
	return ui->lePassword->text();
}

void DlgAddEditUser::accept()
{
	if (m_dialogType == DialogType::Add) {
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
	else if (m_dialogType == DialogType::Edit) {
		ui->lblStatus->setText(tr("Updating user ...") + QString::fromStdString(aclEtcUsersNodePath()));

		if (ui->chbCreateRole->isChecked()) {
			callCreateRole(user(), [this]() { callSetUserSettings(); });
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
	ui->lblStatus->setText(tr("Getting settings ..."));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				ui->lblStatus->setText(QString::fromStdString(response.error().toString()));
			}
			else {
				if (isShv3()) {
					m_user = shv3AclUserFromRpcValue(response.result());
				}
				else {
					m_user = shv::iotqt::acl::AclUser::fromRpcValue(response.result());
				}
				setRoles(m_user.roles);
				ui->lblStatus->setText("");
			}
		}
		else{
			ui->lblStatus->setText(tr("Request timeout expired"));
		}
	});

	ui->lePassword->setText({});
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

	if (!password().isEmpty()) {
		// user wants to change password
		m_user.password.format = shv::iotqt::acl::AclPassword::Format::Sha1;
		m_user.password.password = sha1_hex(password().toStdString());
	}

	RpcValue password_rv;
	if (isShv3()) {
		password_rv = shv3AclUserToRpcValue(m_user);
	} else {
		password_rv = m_user.toRpcValue();
	}

	shv::chainpack::RpcValue::List params{user(), password_rv};
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

namespace {
constexpr auto PASSWORD = "password";
constexpr auto PLAIN = "Plain";
constexpr auto SHA1 = "Sha1";
}
shv::iotqt::acl::AclUser DlgAddEditUser::shv3AclUserFromRpcValue(const shv::chainpack::RpcValue &v)
{
	/*
	SHV3
	{
	  "password":{"Plain":"viewer"},
	  "roles":["subscribe", "browse"]
	}
	*/
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

shv::chainpack::RpcValue DlgAddEditUser::shv3AclUserToRpcValue(const shv::iotqt::acl::AclUser &user)
{
	using namespace shv::iotqt::acl;
	RpcValue::Map ret;
	switch (user.password.format) {
	case shv::iotqt::acl::AclPassword::Format::Invalid:
		break;
	case shv::iotqt::acl::AclPassword::Format::Plain:
		ret[PASSWORD] = RpcValue::Map{{PLAIN, user.password.password}};
		break;
	case shv::iotqt::acl::AclPassword::Format::Sha1:
		ret[PASSWORD] = RpcValue::Map{{SHA1, user.password.password}};
		break;
	}
	ret["roles"] = user.roles;
	return ret;
}
