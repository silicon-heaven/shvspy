#include "dlgroleseditor.h"
#include "ui_dlgroleseditor.h"

#include "dlgaddeditrole.h"
#include "shv/core/assert.h"
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

static const std::string SET_VALUE_METHOD = "setValue";

DlgRolesEditor::DlgRolesEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection) :
	QDialog(parent),
	ui(new Ui::DlgRolesEditor)
{
	ui->setupUi(this);

	SHV_ASSERT_EX(rpc_connection != nullptr, "RPC connection is NULL");

	m_rpcConnection = rpc_connection;

	static constexpr double ROW_HEIGHT_RATIO = 1.3;
	static QStringList INFO_HEADER_NAMES {{ tr("Role") }};

	m_dataModel = new QStandardItemModel(this);
	m_dataModel->setColumnCount(static_cast<int>(INFO_HEADER_NAMES.count()));
	m_dataModel->setHorizontalHeaderLabels(INFO_HEADER_NAMES);

	m_modelProxy = new QSortFilterProxyModel(this);
	m_modelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_modelProxy->setSourceModel(m_dataModel);
	ui->twRoles->setModel(m_modelProxy);

	ui->twRoles->horizontalHeader()->setStretchLastSection(true);
	ui->twRoles->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twRoles->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twRoles->verticalHeader()->setVisible(false);

	connect(ui->pbAddRole, &QPushButton::clicked, this, &DlgRolesEditor::onAddRoleClicked);
	connect(ui->pbEditRole, &QPushButton::clicked, this, &DlgRolesEditor::onEditRoleClicked);
	connect(ui->twRoles, &QTableView::doubleClicked, this, &DlgRolesEditor::onTableRoleDoubleClicked);
	connect(ui->leFilter, &QLineEdit::textChanged, m_modelProxy, &QSortFilterProxyModel::setFilterFixedString);

	setStatusText(QString());
}

DlgRolesEditor::~DlgRolesEditor()
{
	delete ui;
}

void DlgRolesEditor::init(const std::string &acl_node_path)
{
	m_aclEtcNodePath = acl_node_path;
	listRoles();
}

std::string DlgRolesEditor::aclEtcRolesNodePath()
{
	return m_aclEtcNodePath + "/roles";
}

std::string DlgRolesEditor::aclEtcAccessNodePath()
{
	return m_aclEtcNodePath + "/access";
}

QString DlgRolesEditor::selectedRole()
{
	return (ui->twRoles->currentIndex().isValid()) ? ui->twRoles->currentIndex().data().toString() : QString();
}

void DlgRolesEditor::onAddRoleClicked()
{
	auto dlg = new DlgAddEditRole(this, m_rpcConnection, m_aclEtcNodePath, DlgAddEditRole::DialogType::Add);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listRoles();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgRolesEditor::onEditRoleClicked()
{
	QString role = selectedRole();

	if (role.isEmpty()){
		setStatusText(tr("Select role in the table above."));
		return;
	}

	setStatusText(QString());

	auto dlg = new DlgAddEditRole(this, m_rpcConnection, m_aclEtcNodePath, DlgAddEditRole::DialogType::Edit);
	dlg->init(role);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listRoles();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgRolesEditor::onTableRoleDoubleClicked(QModelIndex ix)
{
	Q_UNUSED(ix);
	onEditRoleClicked();
}

void DlgRolesEditor::listRoles()
{
	if (m_rpcConnection == nullptr)
		return;

	m_dataModel->removeRows(0, m_dataModel->rowCount());

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Failed to load roles.") + " " + QString::fromStdString(response.error().toString()));
			}
			else{
				if (response.result().isList()){
					const auto result = response.result();
					const auto &res = result.asList();
					m_dataModel->setRowCount(static_cast<int>(res.size()));
					for (size_t i = 0; i < res.size(); i++){
						auto *item = new QStandardItem(QString::fromStdString(res.at(i).toStdString()));
						item->setFlags(item->flags() & ~Qt::ItemIsEditable);
						m_dataModel->setItem(static_cast<int>(i), 0, item);
					}
				}
				setStatusText(QString());
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcRolesNodePath(), shv::chainpack::Rpc::METH_LS);
}

void DlgRolesEditor::callDeleteAccessForRole(const std::string &role)
{
	if (m_rpcConnection == nullptr)
		return;

	setStatusText(tr("Deleting access paths for role:") + " " + QString::fromStdString(role));

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()){
			if(response.isError()) {
				setStatusText(tr("Failed to delete access pahts.") + " " + QString::fromStdString(response.error().toString()));
			}
			else{
				setStatusText(QString());
			}
		}
		else{
			setStatusText(tr("Request timeout expired"));
		}
	});

	shv::chainpack::RpcValue::List params{shv::chainpack::RpcValue::String(role), {}};
	m_rpcConnection->callShvMethod(rqid, aclEtcAccessNodePath(), SET_VALUE_METHOD, params);
}

void DlgRolesEditor::setStatusText(const QString &txt)
{
	if(txt.isEmpty()) {
		ui->lblStatus->hide();
	}
	else {
		ui->lblStatus->show();
		ui->lblStatus->setText(txt);
	}
}
