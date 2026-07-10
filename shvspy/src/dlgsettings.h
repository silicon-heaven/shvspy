#pragma once

#include <shv/iotqt/rpc/clientconnection.h>

#include <QDialog>

namespace Ui {
class DlgSettings;
}

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

	void setStatusText(const QString &txt);
	void onBrokerConnectedChanged(bool is_connected);

	void load();

	void loadUsers(std::function<void(bool)> callback);
	void reloadUsers();
	void clearUsers();
	void onAddUserClicked();
	void onEditUserClicked();
	void onDeleteUserClicked();

	void loadRoles(std::function<void(bool)> callback);
	void reloadRoles();
	void clearRoles();
	void onAddRoleClicked();
	void onEditRoleClicked();
	void onDeleteRoleClicked();

	void loadMounts(std::function<void(bool)> callback);
	void reloadMounts();
	void clearMounts();
	void onAddMountClicked();
	void onDeleteMountClicked();
	void onEditMountClicked();

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

	QStandardItemModel *m_rolesDataModel;
	QSortFilterProxyModel *m_rolesModelProxy;

	QStandardItemModel *m_mountsDataModel;
	QSortFilterProxyModel *m_mountsModelProxy;
};
