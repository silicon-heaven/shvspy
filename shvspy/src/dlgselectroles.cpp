#include "dlgselectroles.h"

#include "dlgaddeditrole.h"
#include "ui_dlgselectroles.h"

#include <QMenu>

DlgSelectRoles::DlgSelectRoles(QWidget *parent):
	QDialog(parent),
	ui(new Ui::DlgSelectRoles)
{
	ui->setupUi(this);
}

void DlgSelectRoles::init(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_node_path, const std::vector<std::string> &roles)
{
    m_rpcConnection = rpc_connection;
    m_aclEtcNodePath = acl_etc_node_path;

	m_rolesTreeModel = new RolesTreeModel(this);
	m_rolesTreeModel->load(rpc_connection, aclEtcRolesNodePath());
	ui->tvRoles->setModel(m_rolesTreeModel);
    ui->tvRoles->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	m_userRoles = roles;

    connect(ui->tvRoles, &QTreeView::customContextMenuRequested, this, &DlgSelectRoles::contextMenu);
    connect(ui->tvRoles->selectionModel(), &QItemSelectionModel::currentChanged, [this]() {
        ui->editRoleButton->setEnabled(ui->tvRoles->currentIndex().isValid());
    });
    connect(ui->editRoleButton, &QPushButton::clicked, this, &DlgSelectRoles::editRole);

    ui->lblStatus->setText(tr("Loading..."));
    ui->tvRoles->setEnabled(false);
    ui->editRoleButton->setEnabled(false);

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

std::vector<std::string> DlgSelectRoles::selectedRoles()
{
	return m_rolesTreeModel->selectedRoles();
}

void DlgSelectRoles::setUserRoles(const std::vector<std::string> &roles)
{
    m_rolesTreeModel->setSelectedRoles(roles);
}

void DlgSelectRoles::contextMenu(const QPoint &glob_pos)
{
    QMenu menu(this);
    auto *edit_role = new QAction(tr("&Edit role"), &menu);
    connect(edit_role, &QAction::triggered, this, &DlgSelectRoles::editRole);
    edit_role->setEnabled(ui->tvRoles->currentIndex().isValid());
    menu.addAction(edit_role);
    menu.exec(mapToGlobal(glob_pos));
}

void DlgSelectRoles::editRole()
{
    QStandardItem *item = m_rolesTreeModel->itemFromIndex(ui->tvRoles->currentIndex());
    QString role_name = item->data(RolesTreeModel::NameRole).toString();

    auto dlg = new DlgAddEditRole (this, m_rpcConnection, m_aclEtcNodePath, DlgAddEditRole::DialogType::Edit);
	dlg->init(role_name);
	connect(dlg, &QDialog::finished, dlg, [this, dlg, item] (int result) mutable {
        if (result == QDialog::Accepted) {
            m_currentItemPath << item->text();
            while (item->parent()) {
                item = item->parent();
                m_currentItemPath.prepend(item->text());
            }
            m_userRoles = selectedRoles();

            m_rolesTreeModel->clear();
            ui->tvRoles->setEnabled(false);
            ui->lblStatus->setText(tr("Reloading roles..."));
            ui->editRoleButton->setEnabled(false);
            m_rolesTreeModel->load(m_rpcConnection, aclEtcRolesNodePath());
        }

		dlg->deleteLater();
	});
    dlg->open();
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
