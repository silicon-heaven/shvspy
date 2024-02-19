#ifndef DLGADDEDITROLE_H
#define DLGADDEDITROLE_H

#include "accessmodel/accessmodel.h"

#include <shv/chainpack/rpcvalue.h>
#include <shv/iotqt/acl/aclrole.h>

#include <QDialog>
#include <QTableView>

namespace Ui {
class DlgAddEditRole;
}

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgAddEditRole : public QDialog
{
	Q_OBJECT

public:
	enum class DialogType {Add = 0, Edit, Count};

	explicit DlgAddEditRole(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, DlgAddEditRole::DialogType dt = DialogType::Add);
	~DlgAddEditRole() override;

	DialogType dialogType();
	void init(const QString &role_name);
	void accept() Q_DECL_OVERRIDE;

private:
	void callSetRoleSettings();
	void callGetRoleSettings();
	void checkExistingRole(std::function<void(bool, bool)> callback);

	void callSetAccessRulesForRole();
	void callGetAccessRulesForRole();

	QString roleName() const;

	std::vector<std::string> roles() const;
	void setRoles(const std::vector<std::string> &roles);

	int weight() const;
	void setWeight(int weight);

	shv::chainpack::RpcValue profile() const;
	void setProfile(const shv::chainpack::RpcValue &p);

	std::string aclEtcRoleNodePath();
	std::string aclEtcAcessNodePath();
	std::string roleShvPath();
	std::string accessShvPath();

	void onAddRowClicked();
	void onDeleteRowClicked();
	void onMoveRowUpClicked();
	void onMoveRowDownClicked();

	void setStatusText(const QString &txt);
private:
	Ui::DlgAddEditRole *ui;
	DialogType m_dialogType;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;
	AccessModel m_accessModel;
	shv::iotqt::acl::AclRole m_role;
};

#endif // DLGADDEDITROLE_H
