#ifndef DLGADDEDITUSER_H
#define DLGADDEDITUSER_H

#include <QDialog>

#include "shv/chainpack/rpcvalue.h"

#include <shv/iotqt/acl/acluser.h>

namespace Ui {
class DlgAddEditUser;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgAddEditUser : public QDialog
{
	Q_OBJECT

public:
	enum class DialogType {Add = 0, Edit, Count};
	explicit DlgAddEditUser(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, DlgAddEditUser::DialogType dt = DialogType::Add);
	~DlgAddEditUser() override;

	DialogType dialogType();
	std::string user();
	void setUser(const QString &user);
	QString password();

	void accept() Q_DECL_OVERRIDE;

private:
	void onShowPasswordClicked();
	void onSelectRolesClicked();
    void execSelectRolesDialog();

	void callCreateRole(const std::string &role_name, std::function<void()> callback);
	void callSetUserSettings();
	void callGetUserSettings();
	void checkExistingUser(std::function<void(bool, bool)> callback);

    std::string aclEtcUsersNodePath();
	std::string aclEtcRolesNodePath();
	std::string userShvPath();

	std::vector<std::string> roles();
	void setRoles(const std::vector<std::string> &roles);
	void setRoles(const shv::chainpack::RpcValue::List &roles);

	Ui::DlgAddEditUser *ui;
	DialogType m_dialogType;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	shv::iotqt::acl::AclUser m_user;
};

#endif // DLGEDITUSER_H
