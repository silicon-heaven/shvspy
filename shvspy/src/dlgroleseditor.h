#pragma once

#include <QDialog>

#include <shv/iotqt/rpc/clientconnection.h>

namespace Ui {
class DlgRolesEditor;
}

class QSortFilterProxyModel;
class QStandardItemModel;

class DlgRolesEditor : public QDialog
{
	Q_OBJECT

public:
	explicit DlgRolesEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &broker_path, const shv::chainpack::IRpcConnection::ShvApiVersion brokerApiVersion);
	~DlgRolesEditor() override;

private:
	std::string aclEtcRolesNodePath();
	std::string aclEtcAccessNodePath();

	QString selectedRole();
	void listRoles();
	void callDeleteAccessForRole(const std::string &role);

	void onAddRoleClicked();
	void onEditRoleClicked();
	void onDeleteRoleClicked();
	void onTableRoleDoubleClicked(QModelIndex ix);

	void setStatusText(const QString &txt);
	std::string aclAccessPath();
private:
	Ui::DlgRolesEditor *ui;
	std::string m_brokerPath;
	shv::chainpack::IRpcConnection::ShvApiVersion m_brokerApiVersion;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
};

