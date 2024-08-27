#include "dlguserseditor.h"
#include "ui_dlguserseditor.h"

#include "dlgaddedituser.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/chainpack/rpcvalue.h>
#include <shv/core/log.h>
#include <shv/core/assert.h>

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableWidgetItem>

static const std::string SET_VALUE_METHOD = "setValue";

DlgUsersEditor::DlgUsersEditor(QWidget *parent, shv::iotqt::rpc::ClientConnection *rpc_connection) :
	QDialog(parent),
	ui(new Ui::DlgUsersEditor)
{
	ui->setupUi(this);

	SHV_ASSERT_EX(rpc_connection != nullptr, "RPC connection is NULL");

	static constexpr double ROW_HEIGHT_RATIO = 1.3;
	static QStringList INFO_HEADER_NAMES {{ tr("User") }};

	m_dataModel = new QStandardItemModel(this);
	m_dataModel->setColumnCount(static_cast<int>(INFO_HEADER_NAMES.count()));
	m_dataModel->setHorizontalHeaderLabels(INFO_HEADER_NAMES);

	m_modelProxy = new QSortFilterProxyModel(this);
	m_modelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_modelProxy->setSourceModel(m_dataModel);
	ui->twUsers->setModel(m_modelProxy);

	ui->twUsers->horizontalHeader()->setStretchLastSection(true);
	ui->twUsers->verticalHeader()->setDefaultSectionSize(static_cast<int>(ui->twUsers->fontMetrics().height() * ROW_HEIGHT_RATIO));
	ui->twUsers->verticalHeader()->setVisible(false);

	m_rpcConnection = rpc_connection;

	connect(ui->pbAddUser, &QPushButton::clicked, this, &DlgUsersEditor::onAddUserClicked);
	connect(ui->pbDeleteUser, &QPushButton::clicked, this, &DlgUsersEditor::onDelUserClicked);
	connect(ui->pbEditUser, &QPushButton::clicked, this, &DlgUsersEditor::onEditUserClicked);
	connect(ui->twUsers, &QTableWidget::doubleClicked, this, &DlgUsersEditor::onTableUsersDoubleClicked);
	connect(ui->leFilter, &QLineEdit::textChanged, m_modelProxy, &QSortFilterProxyModel::setFilterFixedString);

	listUsers();
}

DlgUsersEditor::~DlgUsersEditor()
{
	delete ui;
}

void DlgUsersEditor::listUsers()
{
	if (m_rpcConnection == nullptr)
		return;

	m_dataModel->removeRows(0, m_dataModel->rowCount());

	int rqid = m_rpcConnection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

	cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				ui->lblStatus->setText(tr("Failed to load users.") + " " + QString::fromStdString(response.error().toString()));
			}
			else{
				if (response.result().isList()){
					const auto result = response.result();
					const auto &res = result.asList();
					m_dataModel->setRowCount(static_cast<int>(res.size()));
					for (size_t i = 0; i < res.size(); i++) {
						auto *item = new QStandardItem(QString::fromStdString(res.at(i).asString()));
						item->setFlags(item->flags() & ~Qt::ItemIsEditable);
						m_dataModel->setItem(static_cast<int>(i), 0, item);
					}
				}
			}
		}
		else{
			ui->lblStatus->setText(tr("Request timeout expired"));
		}
	});

	m_rpcConnection->callShvMethod(rqid, aclEtcUsersNodePath(), shv::chainpack::Rpc::METH_LS);
}

QString DlgUsersEditor::selectedUser()
{
	return (ui->twUsers->currentIndex().isValid()) ? ui->twUsers->currentIndex().data().toString() : QString();
}

void DlgUsersEditor::onAddUserClicked()
{
	auto dlg = new DlgAddEditUser(this, m_rpcConnection, aclEtcUsersNodePath(), DlgAddEditUser::DialogType::Add);
	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listUsers();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgUsersEditor::onDelUserClicked()
{
	QString user = selectedUser();

	if (user.isEmpty()){
		ui->lblStatus->setText(tr("Select user in the table above."));
		return;
	}

	ui->lblStatus->setText("");

	if (QMessageBox::question(this, tr("Delete user"), tr("Do you really want to delete user") + " " + user) == QMessageBox::Yes){
		int rqid = m_rpcConnection->nextRequestId();
		auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);

		cb->start(this, [this](const shv::chainpack::RpcResponse &response) {
			if(response.isValid()){
				if(response.isError()) {
					ui->lblStatus->setText(tr("Failed to delete user.") + " " + QString::fromStdString(response.error().toString()));
				}
				else{
					listUsers();
				}
			}
			else{
				ui->lblStatus->setText(tr("Request timeout expired"));
			}
		});

		shv::chainpack::RpcValue::List params{user.toStdString(), {}};
		m_rpcConnection->callShvMethod(rqid, aclEtcUsersNodePath(), SET_VALUE_METHOD, params);
	}
}

void DlgUsersEditor::onEditUserClicked()
{
	QString user = selectedUser();

	if (user.isEmpty()){
		ui->lblStatus->setText(tr("Select user in the table above."));
		return;
	}

	ui->lblStatus->setText("");

	auto dlg = new DlgAddEditUser(this, m_rpcConnection, aclEtcUsersNodePath(), DlgAddEditUser::DialogType::Edit);
	dlg->setUser(user);

	connect(dlg, &QDialog::finished, dlg, [this, dlg] (int result) {
		if (result == QDialog::Accepted){
			listUsers();
		}

		dlg->deleteLater();
	});
	dlg->open();
}

void DlgUsersEditor::onTableUsersDoubleClicked(QModelIndex ix)
{
	Q_UNUSED(ix);
	onEditUserClicked();
}

std::string DlgUsersEditor::aclEtcUsersNodePath()
{
	switch (m_rpcConnection->shvApiVersion()) {
	case shv::chainpack::IRpcConnection::ShvApiVersion::V2: return ".broker/etc/acl/users";
	case shv::chainpack::IRpcConnection::ShvApiVersion::V3: return ".broker/access/users";
	}
	return "";
}
