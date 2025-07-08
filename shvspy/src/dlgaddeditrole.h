#ifndef DLGADDEDITROLE_H
#define DLGADDEDITROLE_H

#include <shv/chainpack/rpcvalue.h>
#include <shv/chainpack/irpcconnection.h>
#include <shv/iotqt/acl/aclrole.h>

#include <QDialog>
#include <QTableView>

namespace Ui {
class DlgAddEditRole;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class AccessModel;

class DlgAddEditRole : public QDialog
{
	Q_OBJECT

public:
	explicit DlgAddEditRole(shv::iotqt::rpc::ClientConnection *rpc_connection,
							const std::string &acl_etc_node_path,
							const QString &role_name,
							QWidget *parent);
	~DlgAddEditRole() override;

	void accept() override;
private:
	bool isV2() const { return m_shvApiVersion == shv::chainpack::IRpcConnection::ShvApiVersion::V2; }
	void loadRole(const QString &role_name);

	void saveRoleAndExitIfSuccess();
	void callGetRoleSettings();
	void checkExistingRole(std::function<void(bool, bool)> callback);

	void saveAccessRulesAndExitIfSuccess();
	void callGetAccessRulesForRole();

	QString roleName() const;

	std::vector<std::string> roles() const;
	void setRoles(const std::vector<std::string> &roles);

	int weight() const;
	void setWeight(int weight);

	shv::chainpack::RpcValue profile() const;
	void setProfile(const shv::chainpack::RpcValue &p);

	std::string aclShvPath();
	std::string rolesShvPath();
	std::string roleShvPath();
	std::string accessShvPath();
	std::string roleAccessShvPath();

	void onAddRowClicked();
	void onDeleteRowClicked();
	void onMoveRowUpClicked();
	void onMoveRowDownClicked();

	void setStatusText(const QString &txt);
private:
	enum class DialogType {Add = 0, Edit, Count};
	DialogType m_dialogType;
	shv::chainpack::IRpcConnection::ShvApiVersion m_shvApiVersion;

	Ui::DlgAddEditRole *ui;

	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	AccessModel *m_accessModel = nullptr;
	shv::iotqt::acl::AclRole m_role;
};

#endif // DLGADDEDITROLE_H
