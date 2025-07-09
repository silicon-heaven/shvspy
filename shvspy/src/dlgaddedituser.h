#ifndef DLGADDEDITUSER_H
#define DLGADDEDITUSER_H

#include <QDialog>

#include <shv/chainpack/irpcconnection.h>
#include <shv/iotqt/acl/acluser.h>

namespace Ui {
class DlgAddEditUser;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgAddEditUser : public QDialog
{
	Q_OBJECT

public:
	explicit DlgAddEditUser(
			shv::iotqt::rpc::ClientConnection *rpc_connection,
			const std::string &acl_etc_node_path,
			const QString &user,
			QWidget *parent
			);
	~DlgAddEditUser() override;

	std::string user();
	QString password();

	void accept() override;

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

	bool isShv3() const { return m_shvApiVersion == shv::chainpack::IRpcConnection::ShvApiVersion::V3; }
private:
	enum class DialogType {Add = 0, Edit, Count};
	DialogType m_dialogType;
	shv::chainpack::IRpcConnection::ShvApiVersion m_shvApiVersion;

	Ui::DlgAddEditUser *ui;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	shv::iotqt::acl::AclUser m_user;
};

#endif // DLGEDITUSER_H
