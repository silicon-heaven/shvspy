#pragma once

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/acl/acluser.h>

#include <optional>
#include <QDialog>

namespace Ui {
class DlgSettings;
}

namespace shv::iotqt::acl {
class AclMountDef;
}

class AccessModel;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;

class DlgSettings : public QDialog
{
	Q_OBJECT
	using Super = QDialog;

public:
	explicit DlgSettings(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &broker_path, const shv::chainpack::IRpcConnection::ShvApiVersion brokerApiVersion, QWidget *parent = nullptr);
	~DlgSettings() override;

private:
	std::string aclAccessPath();
	std::string aclAccessUsersPath();
	std::string aclAccessRolesPath();
	std::string aclAccessMountsPath();
	bool isShv3() const;

	void setStatusText(const QString &txt);
	void onBrokerConnectedChanged(bool is_connected);

	void load();

	QStringList stringListFromLineEdit(QLineEdit *le) const;
	void setStringListToLineEdit(QLineEdit *le, const QStringList &items);

	void loadUsers(std::function<void(bool)> callback);
	void reloadUsers(const QString &user_to_select);
	void clearUsers();
	void onAddUserClicked();
	void onEditUserClicked();
	void onDeleteUserClicked();

	void onSelectRolesClicked();
	void execSelectRolesDialog(QLineEdit *le);
	void callCreateRole(std::function<void(bool)> callback);
	void callGetUser(std::function<void (bool)> callback);
	void callSaveUser(std::function<void (bool)> callback);
	void saveUserEdit(std::function<void (bool)> callback);
	void showUserEdit();
	void hideUserEdit();
	void setUserPasswordMode(bool password_mode);
	void checkExistingUser(std::function<void (bool, bool)> callback);

	void loadRoles(std::function<void(bool)> callback);
	void reloadRoles(const QString &role_to_select);
	void clearRoles();
	void onAddRoleClicked();
	void onEditRoleClicked();
	void onDeleteRoleClicked();

	void showRoleEdit();
	void hideRoleEdit();
	void saveRoleEdit(std::function<void(bool)> callback);
	void callGetRole(std::function<void(bool, const QStringList &, const shv::chainpack::RpcValue &, const std::optional<int> &, const shv::chainpack::RpcValue &)> callback);
	void callSaveRole(std::function<void(bool)> callback);
	void checkExistingRole(std::function<void(bool, bool)> callback);

	void loadMounts(std::function<void(bool)> callback);
	void reloadMounts(const QString &mount_point_to_select);
	void clearMounts();
	void onAddMountClicked();
	void onDeleteMountClicked();
	void onEditMountClicked();

	void showMountEdit();
	void hideMountEdit();
	void callGetMount(std::function<void(bool, const shv::iotqt::acl::AclMountDef &mount_def)> callback);
	void callSaveMount(std::function<void(bool)> callback);
	void checkExistingMount(std::function<void(bool, bool)> callback);
	void saveMountEdit(std::function<void (bool)> callback);

	void callShvMethod(const std::string &path, const std::string &method, const shv::chainpack::RpcValue &params, std::function<void(const shv::chainpack::RpcValue &)> on_success, std::function<void(const QString &)> on_error);

	QString currentRow(QTableView *table) const;
	void setCurrentRow(QTableView *table, const QString &row);
	void sortTable(QTableView *table) const;

	void setUserControlsEnabled(bool enabled);
	void setRoleControlsEnabled(bool enabled);
	void setMountControlsEnabled(bool enabled);
	void setControlsEnabled(bool enabled);

	Ui::DlgSettings *ui;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection;
	std::string m_brokerPath;
	shv::chainpack::IRpcConnection::ShvApiVersion m_brokerApiVersion;

	QStandardItemModel *m_usersDataModel;
	QSortFilterProxyModel *m_usersModelProxy;
	shv::iotqt::acl::AclUser m_editUser;

	QStandardItemModel *m_rolesDataModel;
	QSortFilterProxyModel *m_rolesModelProxy;

	QStandardItemModel *m_mountsDataModel;
	QSortFilterProxyModel *m_mountsModelProxy;

	AccessModel *m_accessModel = nullptr;
};
