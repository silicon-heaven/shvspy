#pragma once

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/acl/acluser.h>

#include <optional>
#include <stop_token>
#include <QDialog>

class QModelIndex;

namespace Ui {
class DlgSettings;
}

namespace shv::iotqt::acl {
class AclMountDef;
}

class AccessModel;
class QLabel;
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
	struct UserAccessRule
	{
		QString path;
		QString grant;
		QString role;
	};

	std::string aclAccessPath();
	std::string aclAccessUsersPath();
	std::string aclAccessRolesPath();
	std::string aclAccessMountsPath();
	bool isShv3() const;

	void setStatusText(const QString &txt);
	void onBrokerConnectedChanged(bool is_connected);

	void load();
	void loadVersionInfo();

	QStringList stringListFromLineEdit(QLineEdit *le) const;
	void setStringListToLineEdit(QLineEdit *le, const QStringList &items);

	void loadUsers(std::function<void(bool)> callback);
	void reloadUsers(const QString &user_to_select);
	void clearUsers();
	void onAddUserClicked();
	void onEditUserClicked();
	void loadUserIntoEditPanel();
	void onUsersCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);
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

	void refreshUserAccessRules();
	void refreshRoleAccessRules();
	void refreshFlattenedAccessRules(const QStringList &initial_roles, QStandardItemModel *model, QSharedPointer<bool> &cancel_token);
	void processNextAccessRule(QStringList queue, QSharedPointer<QSet<QString>> known_roles, QSharedPointer<QList<UserAccessRule>> result_rules, QStandardItemModel *model, QSharedPointer<bool> cancel_token);
	void callGetRoleAccessRules(const QString &role, std::function<void(bool, const QStringList &sub_roles, const shv::chainpack::RpcValue &access)> callback);
	void appendUserAccessRuleRows(const shv::chainpack::RpcValue &access, const QString &role, QList<UserAccessRule> &rows);

	void loadRoles(std::function<void(bool)> callback);
	void reloadRoles(const QString &role_to_select);
	void clearRoles();
	void onAddRoleClicked();
	void onEditRoleClicked();
	void loadRoleIntoEditPanel();
	void onRolesCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);
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
	void loadMountIntoEditPanel();
	void onMountsCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);

	void showMountEdit();
	void hideMountEdit();
	void callGetMount(std::function<void(bool, const shv::iotqt::acl::AclMountDef &mount_def)> callback);
	void callSaveMount(std::function<void(bool)> callback);
	void checkExistingMount(std::function<void(bool, bool)> callback);
	void saveMountEdit(std::function<void (bool)> callback);

	void callShvMethod(const std::string &path, const std::string &method, const shv::chainpack::RpcValue &params, std::function<void(const shv::chainpack::RpcValue &)> on_success, std::function<void(const QString &)> on_error);

	void handleRowSwitchConfirmation(QTableView *table, QWidget *edit_widget, bool &dirty, const QModelIndex &previous,
									  std::function<void(std::function<void(bool)>)> save_funcion,
									  std::function<void()> discard_function,
									  std::function<void(const QString &)> reload_function);

	QString currentRow(QTableView *table) const;
	void setCurrentRow(QTableView *table, const QString &row);
	void sortTable(QTableView *table) const;

	void setUserControlsEnabled(bool enabled);
	void setRoleControlsEnabled(bool enabled);
	void setMountControlsEnabled(bool enabled);
	void setControlsEnabled(bool enabled);

	Ui::DlgSettings *ui;
	QLabel *m_lblVersions;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection;
	std::string m_brokerPath;
	shv::chainpack::IRpcConnection::ShvApiVersion m_brokerApiVersion;

	QStandardItemModel *m_usersDataModel;
	QSortFilterProxyModel *m_usersModelProxy;
	shv::iotqt::acl::AclUser m_editUser;
	QStandardItemModel *m_userAccessRulesModel;
	QSharedPointer<bool> m_userAccessRulesCancelToken;
	bool m_userEditDirty = false;

	QStandardItemModel *m_rolesDataModel;
	QSortFilterProxyModel *m_rolesModelProxy;
	QStandardItemModel *m_roleAccessRulesModel;
	QSharedPointer<bool> m_roleAccessRulesCancelToken;
	bool m_roleEditDirty = false;

	QStandardItemModel *m_mountsDataModel;
	QSortFilterProxyModel *m_mountsModelProxy;
	bool m_mountEditDirty = false;

	bool m_ignoreRowChange = false;

	AccessModel *m_accessModel = nullptr;
};
