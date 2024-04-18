#pragma once

#include <QDialog>

namespace Ui {
class DlgRolesEditor;
}

class QSortFilterProxyModel;
class QStandardItemModel;

namespace shv::iotqt::rpc { class ClientConnection; }

class DlgRolesEditor : public QDialog
{
	Q_OBJECT

public:
	explicit DlgRolesEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection);
	~DlgRolesEditor() override;
	void init(const std::string &acl_node_path);

private:
	std::string aclEtcRolesNodePath();
	std::string aclEtcAccessNodePath();

	QString selectedRole();
	void listRoles();
	void callDeleteAccessForRole(const std::string &role);

	void onAddRoleClicked();
	void onEditRoleClicked();
	void onTableRoleDoubleClicked(QModelIndex ix);

	void setStatusText(const QString &txt);

private:
	Ui::DlgRolesEditor *ui;
	std::string m_aclEtcNodePath;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	QStandardItemModel *m_dataModel;
	QSortFilterProxyModel *m_modelProxy;
};

