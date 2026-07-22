#ifndef DLGSELECTROLES_H
#define DLGSELECTROLES_H

#include <QDialog>

#include "rolestreemodel/rolestreemodel.h"

#include <shv/iotqt/rpc/clientconnection.h>

namespace Ui {
class DlgSelectRoles;
}

class DlgSelectRoles : public QDialog
{
	Q_OBJECT

public:
	explicit DlgSelectRoles(QWidget *parent);
	~DlgSelectRoles() override;

	void init(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, const QStringList &roles);

	QStringList selectedRoles();
	void setUserRoles(const QStringList &roles);


private:
	void contextMenu(const QPoint &glob_pos);
	void editRole();
	QStandardItem *findChildItem(QStandardItem *item, const QStringList &path, int ix = 0);
	QStandardItem *findChildItem(QStandardItem *item, const QString &text);
	std::string aclEtcRolesNodePath();

	QStringList m_userRoles;
	QStringList m_currentItemPath;
	RolesTreeModel *m_rolesTreeModel = nullptr;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	std::string m_aclEtcNodePath;

	Ui::DlgSelectRoles *ui;
};

#endif // DLGDLGSELECTROLES_H
