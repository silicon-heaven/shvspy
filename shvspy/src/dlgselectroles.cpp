#include "dlgselectroles.h"

#include "ui_dlgselectroles.h"

#include <QMenu>

DlgSelectRoles::DlgSelectRoles(QWidget *parent):
	QDialog(parent),
	ui(new Ui::DlgSelectRoles)
{
	ui->setupUi(this);
	ui->splitter->setStretchFactor(0, 2);
	ui->splitter->setStretchFactor(1, 1);

	connect(ui->tbMoveRoleUp, &QToolButton::clicked, this, [this]() { moveSelectedRole(-1); });
	connect(ui->tbMoveRoleDown, &QToolButton::clicked, this, [this]() { moveSelectedRole(1); });
	connect(ui->lstSelectedRoles, &QListWidget::currentRowChanged, this, &DlgSelectRoles::updateMoveButtons);
	updateMoveButtons();
}

void DlgSelectRoles::init(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, const QStringList &roles)
{
	m_rpcConnection = rpc_connection;
	m_aclEtcNodePath = acl_etc_node_path;

	m_rolesTreeModel = new RolesTreeModel(this);
	m_rolesTreeModel->load(rpc_connection, aclEtcRolesNodePath());
	ui->tvRoles->setModel(m_rolesTreeModel);
	ui->lstSelectedRoles->addItems(roles);

	ui->lblStatus->setText(tr("Loading..."));
	ui->tvRoles->setEnabled(false);

	connect(m_rolesTreeModel, &RolesTreeModel::loadFinished, this, [this, roles](){
		ui->lblStatus->clear();
		m_rolesTreeModel->setSelectedRoles(roles);
		ui->tvRoles->setEnabled(true);
		syncSelectedRolesList();

		if (!m_currentItemPath.isEmpty()) {
			QStandardItem *item = findChildItem(m_rolesTreeModel->invisibleRootItem(), m_currentItemPath);
			if (item) {
				QModelIndex ix = m_rolesTreeModel->indexFromItem(item);
				ui->tvRoles->setCurrentIndex(ix);
				ui->tvRoles->scrollTo(ix);
			}
		}
	});

	connect(m_rolesTreeModel, &QStandardItemModel::itemChanged, this, &DlgSelectRoles::syncSelectedRolesList);

	connect(m_rolesTreeModel, &RolesTreeModel::loadError, this, [this](QString error){
		ui->lblStatus->setText(error);
	});
}

QStringList DlgSelectRoles::selectedRoles()
{
	QStringList roles;
	for (int i = 0; i < ui->lstSelectedRoles->count(); ++i) {
		roles << ui->lstSelectedRoles->item(i)->text();
	}
	return roles;
}

void DlgSelectRoles::syncSelectedRolesList()
{
	if (!ui->tvRoles->isEnabled()) {  //do nothing while model loading
		return;
	}

	const QStringList checked_roles = m_rolesTreeModel->selectedRoles();

	for (auto i = ui->lstSelectedRoles->count() - 1; i >= 0; --i) {
		const QString &role = ui->lstSelectedRoles->item(i)->text();
		if (!checked_roles.contains(role)) {
			delete ui->lstSelectedRoles->takeItem(i);
		}
	}
	auto already_selected_roles = selectedRoles();
	for (const auto &role : checked_roles) {
		if (!already_selected_roles.contains(role)) {
			ui->lstSelectedRoles->addItem(role);
		}
	}
	updateMoveButtons();
}

void DlgSelectRoles::moveSelectedRole(int offset)
{
	int row = ui->lstSelectedRoles->currentRow();
	int new_row = row + offset;
	if (row < 0 || new_row < 0 || new_row >= ui->lstSelectedRoles->count()) {
		return;
	}
	ui->lstSelectedRoles->insertItem(new_row, ui->lstSelectedRoles->takeItem(row));
	ui->lstSelectedRoles->setCurrentRow(new_row);
}

void DlgSelectRoles::updateMoveButtons()
{
	int row = ui->lstSelectedRoles->currentRow();
	ui->tbMoveRoleUp->setEnabled(row > 0);
	ui->tbMoveRoleDown->setEnabled(row >= 0 && row < ui->lstSelectedRoles->count() - 1);
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
