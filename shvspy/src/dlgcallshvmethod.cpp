#include "dlgcallshvmethod.h"
#include "ui_dlgcallshvmethod.h"

#include <shv/chainpack/rpcmessage.h>
#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/rpccall.h>

#include <QCoreApplication>
#include <QLineEdit>

namespace cp = shv::chainpack;

DlgCallShvMethod::DlgCallShvMethod(shv::iotqt::rpc::ClientConnection *connection, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::DlgCallShvMethod)
	, m_connection(connection)
{
	ui->setupUi(this);
	connect(ui->btCall, &QPushButton::clicked, this, &DlgCallShvMethod::callShvMethod);

	QCoreApplication *app = QCoreApplication::instance();
	ui->edShvPath->addItems(app->property("DlgCallShvMethod/shvPaths").toStringList());
	ui->edMethod->addItems(app->property("DlgCallShvMethod/methods").toStringList());
	ui->edParams->addItems(app->property("DlgCallShvMethod/params").toStringList());
}

DlgCallShvMethod::~DlgCallShvMethod()
{
	auto save_list = [](const char *prop_name, QComboBox *combo)
	{
		QCoreApplication *app = QCoreApplication::instance();
		QStringList sl;
		for (int i = 0; i < combo->count(); ++i)
			sl << combo->itemText(i);
		app->setProperty(prop_name, sl);
	};
	save_list("DlgCallShvMethod/shvPaths", ui->edShvPath);
	save_list("DlgCallShvMethod/methods", ui->edMethod);
	save_list("DlgCallShvMethod/params", ui->edParams);
	delete ui;
}

void DlgCallShvMethod::setShvPath(const std::string &path)
{
	ui->edShvPath->lineEdit()->setText(QString::fromStdString(path));
}

void DlgCallShvMethod::callShvMethod()
{
	std::string shv_path = ui->edShvPath->currentText().toStdString();
	/*
	if(shv_path.empty()) {
		ui->txtResponse->setPlainText(tr("SHV path cannot be empty!"));
		return;
	}
	*/
	std::string method = ui->edMethod->currentText().toStdString();
	/*
	if(method.empty()) {
		ui->txtResponse->setPlainText(tr("Method cannot be empty!"));
		return;
	}
	*/
	cp::RpcValue params;
	std::string str = ui->edParams->currentText().toStdString();
	std::string err;
	if(!str.empty())
		params = cp::RpcValue::fromCpon(str, &err);
	if(!err.empty()) {
		ui->txtResponse->setPlainText(QString::fromStdString(err));
		return;
	}
	int rq_id = m_connection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_connection, rq_id, this);
	cb->start(this, [this](const cp::RpcResponse &resp) {
		if(resp.isValid()) {
			if(resp.isError())
				ui->txtResponse->setPlainText(tr("RPC request error: %1").arg(QString::fromStdString(resp.error().toString())));
			else
				ui->txtResponse->setPlainText(QString::fromStdString(resp.result().toCpon()));
		}
		else {
			ui->txtResponse->setPlainText(tr("RPC request timeout"));
		}
	});
	m_connection->callShvMethod(rq_id, shv_path, method, params);
}
