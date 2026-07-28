#include "dlgselectroles.h"

#include "ui_dlgselectroles.h"

#include <QMenu>

DlgSelectRoles::DlgSelectRoles(QWidget *parent):
	QDialog(parent),
	ui(new Ui::DlgSelectRoles)
{
	ui->setupUi(this);
}

void DlgSelectRoles::init(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, const QStringList &roles)
{
	m_rpcConnection = rpc_connection;
	m_aclEtcNodePath = acl_etc_node_path;

	m_rolesTreeModel = new RolesTreeModel(this);
	m_rolesTreeModel->load(rpc_connection, aclEtcRolesNodePath());
	ui->tvRoles->setModel(m_rolesTreeModel);
	m_userRoles = roles;

	ui->lblStatus->setText(tr("Loading..."));
	ui->tvRoles->setEnabled(false);

	connect(m_rolesTreeModel, &RolesTreeModel::loadFinished, this, [this](){
		ui->lblStatus->clear();
		ui->tvRoles->setEnabled(true);
		m_rolesTreeModel->setSelectedRoles(m_userRoles);

		if (!m_currentItemPath.isEmpty()) {
			QStandardItem *item = findChildItem(m_rolesTreeModel->invisibleRootItem(), m_currentItemPath);
			if (item) {
				QModelIndex ix = m_rolesTreeModel->indexFromItem(item);
				ui->tvRoles->setCurrentIndex(ix);
				ui->tvRoles->scrollTo(ix);
			}
		}
	});

	connect(m_rolesTreeModel, &RolesTreeModel::loadError, this, [this](QString error){
		ui->lblStatus->setText(error);
	});
}

QStringList DlgSelectRoles::selectedRoles()
{
	return m_rolesTreeModel->selectedRoles();
}

void DlgSelectRoles::setUserRoles(const QStringList &roles)
{
	m_rolesTreeModel->setSelectedRoles(roles);
}

QStandardItem *DlgSelectRoles::findChildItem(QStandardItem *item, const QStringList &path, int ix)
{
	QStandardItem *child = findChildItem(item, path[ix]);
	if (!child) {
		return nullptr;
	}
	if (++ix == path.count()) {
		return child;
	}
	return findChildItem(child, path, ix);
}

QStandardItem *DlgSelectRoles::findChildItem(QStandardItem *item, const QString &text)
{
	for (int i = 0; i < item->rowCount(); ++i) {
		QStandardItem *child = item->child(i, 0);
		if (child->text() == text) {
			return child;
		}
	}
	return nullptr;
}

std::string DlgSelectRoles::aclEtcRolesNodePath()
{
	return m_aclEtcNodePath + "/roles";
}

DlgSelectRoles::~DlgSelectRoles()
{
	delete ui;
}
