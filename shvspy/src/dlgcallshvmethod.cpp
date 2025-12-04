#include "dlgcallshvmethod.h"
#include "ui_dlgcallshvmethod.h"

#include <shv/chainpack/rpcmessage.h>
#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/rpccall.h>

#include <QCoreApplication>
#include <QLineEdit>

using namespace shv::chainpack;

namespace {
const auto Key_shvPaths = "DlgCallShvMethod/shvPaths";
const auto Key_methods = "DlgCallShvMethod/methods";
const auto Key_params = "DlgCallShvMethod/params";
}

DlgCallShvMethod::DlgCallShvMethod(shv::iotqt::rpc::ClientConnection *connection, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::DlgCallShvMethod)
	, m_connection(connection)
{
	ui->setupUi(this);
	connect(ui->btCall, &QPushButton::clicked, this, &DlgCallShvMethod::callShvMethod);

	QCoreApplication *app = QCoreApplication::instance();
	ui->edShvPath->addItems(app->property(Key_shvPaths).toStringList());
	ui->edShvPath->lineEdit()->setClearButtonEnabled(true);
	ui->edMethod->addItems(app->property(Key_methods).toStringList());
	ui->edMethod->lineEdit()->setClearButtonEnabled(true);
	ui->edParams->addItems(app->property(Key_params).toStringList());
	ui->edParams->lineEdit()->setClearButtonEnabled(true);
}

DlgCallShvMethod::~DlgCallShvMethod()
{
	auto save_list = [](const char *prop_name, QComboBox *combo)
	{
		QCoreApplication *app = QCoreApplication::instance();
		QStringList sl;
		if (auto s = combo->currentText().trimmed(); !s.isEmpty()) {
			sl << s;
		}
		for (int i = 0; i < combo->count(); ++i) {
			auto s = combo->itemText(i);
			if (!sl.contains(s)) {
				sl << s;
			}
		}
		app->setProperty(prop_name, sl);
	};
	save_list(Key_shvPaths, ui->edShvPath);
	save_list(Key_methods, ui->edMethod);
	save_list(Key_params, ui->edParams);
	delete ui;
}

void DlgCallShvMethod::setShvPath(const std::string &path)
{
	ui->edShvPath->lineEdit()->setText(QString::fromStdString(path));
}

void DlgCallShvMethod::callShvMethod()
{
	std::string shv_path = ui->edShvPath->currentText().trimmed().toStdString();
	std::string method = ui->edMethod->currentText().trimmed().toStdString();
	RpcValue params;
	std::string str = ui->edParams->currentText().trimmed().toStdString();
	std::string err;
	if(!str.empty())
		params = RpcValue::fromCpon(str, &err);
	if(!err.empty()) {
		ui->txtResponse->setPlainText(QString::fromStdString(err));
		return;
	}
	auto *rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
			->setShvPath(shv_path)
			->setMethod(method)
			->setParams(params);
	if (ui->cbxUserId->isChecked()) {
		rpc_call->setUserId({});
	}
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const RpcValue &result, const RpcError &error) {
		if (error.isValid()) {
			ui->txtResponse->setPlainText(tr("RPC request error: %1").arg(QString::fromStdString(error.toString())));
		} else {
			ui->txtResponse->setPlainText(QString::fromStdString(result.toCpon()));
		}
	});
	rpc_call->start();
}
