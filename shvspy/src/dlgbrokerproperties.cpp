#include "dlgbrokerproperties.h"
#include "ui_dlgbrokerproperties.h"
#include "brokerproperty.h"

#include <shv/chainpack/irpcconnection.h>
#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/socket.h>

#include <QSettings>

#include <shv/core/log.h>

using namespace shv::chainpack;

DlgBrokerProperties::DlgBrokerProperties(QWidget *parent) :
	Super(parent),
	ui(new Ui::DlgBrokerProperties)
{
	ui->setupUi(this);
#if QT_VERSION_MAJOR < 6
	ui->cbLoginWithAzure->hide();
#endif

	{
		using namespace shv::iotqt::rpc;
		using Scheme = Socket::Scheme;
		auto *cbx = ui->cbxScheme;
		cbx->addItem(Socket::schemeToString(Scheme::Tcp), static_cast<int>(Scheme::Tcp));
		cbx->addItem(Socket::schemeToString(Scheme::WebSocket), static_cast<int>(Scheme::WebSocket));
		cbx->addItem(Socket::schemeToString(Scheme::WebSocketSecure), static_cast<int>(Scheme::WebSocketSecure));
		cbx->addItem(Socket::schemeToString(Scheme::SerialPort), static_cast<int>(Scheme::SerialPort));
		cbx->addItem(Socket::schemeToString(Scheme::LocalSocket), static_cast<int>(Scheme::LocalSocket));
		cbx->addItem(Socket::schemeToString(Scheme::LocalSocketSerial), static_cast<int>(Scheme::LocalSocketSerial));
		cbx->setCurrentIndex(0);
	}
	connect(ui->cbxScheme, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] (int ix) {
		using namespace shv::iotqt::rpc;
		using Scheme = Socket::Scheme;
		auto scheme = static_cast<Scheme>(ui->cbxScheme->itemData(ix).toInt());
		ui->edPort->setEnabled(scheme == Scheme::Tcp || scheme == Scheme::WebSocket || scheme == Scheme::WebSocketSecure);
	});

	ui->cbxConnectionType->addItem(tr("Client"), "client");
	ui->cbxConnectionType->addItem(tr("Device"), "device");
	connect(ui->cbxConnectionType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int ix) {
		ui->grpDevice->setEnabled(ix == 1);
	});
	ui->grpDevice->setEnabled(false);

	ui->cbxConnectionType->setCurrentIndex(0);

	ui->rpc_protocolType->addItem(Rpc::protocolTypeToString(Rpc::ProtocolType::ChainPack), static_cast<int>(Rpc::ProtocolType::ChainPack));
	//ui->rpc_protocolType->addItem(Rpc::protocolTypeToString(Rpc::ProtocolType::Cpon), static_cast<int>(Rpc::ProtocolType::Cpon));
	//ui->rpc_protocolType->addItem(Rpc::protocolTypeToString(Rpc::ProtocolType::JsonRpc), static_cast<int>(Rpc::ProtocolType::JsonRpc));
	ui->rpc_protocolType->setCurrentIndex(0);

	using shv::iotqt::rpc::ClientConnection;
	ui->lstSecurityType->addItem(tr("None"), false);
	ui->lstSecurityType->addItem("SSL", true);
	ui->lstSecurityType->setCurrentIndex(0);
	ui->chkPeerVerify->setChecked(false);
	ui->chkPeerVerify->setDisabled(true);

	connect(ui->cbLoginWithAzure, &QCheckBox::clicked, this, [this]() {
		ui->edUser->setEnabled(!ui->cbLoginWithAzure->isChecked());
		ui->edPassword->setEnabled(!ui->cbLoginWithAzure->isChecked());
	});
	connect(ui->lstSecurityType, &QComboBox::currentTextChanged, this,
			[this] (const QString &security_type_text) { ui->chkPeerVerify->setDisabled(security_type_text == "none"); });

	QSettings settings;
	restoreGeometry(settings.value(QStringLiteral("ui/dlgServerProperties/geometry")).toByteArray());
}

DlgBrokerProperties::~DlgBrokerProperties()
{
	delete ui;
}

QVariantMap DlgBrokerProperties::brokerProperties() const
{
	using namespace brokerProperty;
	QVariantMap ret;
	ret[SCHEME] = ui->cbxScheme->currentText();
	ret[NAME] = ui->edName->text();
	ret[HOST] = ui->edHost->text();
	ret[PORT] = ui->edPort->value();
	ret[USER] = ui->edUser->text();
	ret[PASSWORD] = ui->edPassword->text();
	ret[PLAIN_TEXT_PASSWORD] = ui->cbPlainTextPassword->isChecked();
	ret[AZURELOGIN] = ui->cbLoginWithAzure->isChecked();
	ret[SKIPLOGINPHASE] = !ui->grpLogin->isChecked();
	ret[SECURITYTYPE] = ui->lstSecurityType->currentText();
	ret[PEERVERIFY] = ui->chkPeerVerify->isChecked();
	ret[CONNECTIONTYPE] = ui->cbxConnectionType->currentData().toString();
	ret[RPC_PROTOCOLTYPE] = ui->rpc_protocolType->currentData().toInt();
	ret[RPC_RECONNECTINTERVAL] = ui->rpc_reconnectInterval->value();
	ret[RPC_HEARTBEATINTERVAL] = ui->rpc_heartbeatInterval->value();
	ret[RPC_RPCTIMEOUT] = ui->rpc_timeout->value();
	ret[DEVICE_ID] = ui->device_id->text().trimmed();
	ret[DEVICE_MOUNTPOINT] = ui->device_mountPoint->text().trimmed();
	ret[SUBSCRIPTIONS] = m_subscriptions;
	ret[MUTEHEARTBEATS] = ui->chkMuteHeartBeats->isChecked();
	ret[SHVROOT] = ui->shvRoot->text();
	return ret;
}

void DlgBrokerProperties::setBrokerProperties(const QVariantMap &props)
{
	using namespace brokerProperty;

	m_subscriptions = props.value(SUBSCRIPTIONS).toList();

	ui->cbxScheme->setCurrentText(props.value(SCHEME).toString());
	if(ui->cbxScheme->currentIndex() < 0)
		ui->cbxScheme->setCurrentIndex(0);
	ui->edName->setText(props.value(NAME).toString());
	ui->grpLogin->setChecked(!props.value(SKIPLOGINPHASE).toBool());
	ui->edHost->setText(props.value(HOST).toString());
	ui->edPort->setValue(props.value(PORT, shv::chainpack::IRpcConnection::DEFAULT_RPC_BROKER_PORT_NONSECURED).toInt());
	ui->edUser->setText(props.value(USER).toString());
	ui->edPassword->setText(props.value(PASSWORD).toString());
	ui->cbPlainTextPassword->setChecked(props.value(PLAIN_TEXT_PASSWORD).toBool());
	ui->cbLoginWithAzure->setChecked(props.value(AZURELOGIN).toBool());
	ui->edUser->setEnabled(!ui->cbLoginWithAzure->isChecked());
	ui->edPassword->setEnabled(!ui->cbLoginWithAzure->isChecked());

	{
		QVariant v = props.value(RPC_RECONNECTINTERVAL);
		if(v.isValid())
			ui->rpc_reconnectInterval->setValue(v.toInt());
	}
	{
		QVariant v = props.value(RPC_HEARTBEATINTERVAL);
		if(v.isValid())
			ui->rpc_heartbeatInterval->setValue(v.toInt());
	}
	{
		QVariant v = props.value(RPC_RPCTIMEOUT);
		if(v.isValid())
			ui->rpc_timeout->setValue(v.toInt());
	}
	{
		QVariant v = props.value(DEVICE_ID);
		if(v.isValid())
			ui->device_id->setText(v.toString());
	}
	{
		QVariant v = props.value(DEVICE_MOUNTPOINT);
		if(v.isValid())
			ui->device_mountPoint->setText(v.toString());
	}

	QString security_type = props.value(SECURITYTYPE).toString();
	for (int i = 0; i < ui->lstSecurityType->count(); ++i) {
		if (ui->lstSecurityType->itemText(i) == security_type) {
			ui->lstSecurityType->setCurrentIndex(i);
			break;
		}
	}

	ui->chkPeerVerify->setChecked(props.value(PEERVERIFY).toBool());

	QString conn_type = props.value(CONNECTIONTYPE).toString();
	ui->cbxConnectionType->setCurrentIndex(0);
	for (int i = 0; i < ui->cbxConnectionType->count(); ++i) {
		if(ui->cbxConnectionType->itemData(i).toString() == conn_type) {
			ui->cbxConnectionType->setCurrentIndex(i);
			break;
		}
	}

	int proto_type = props.value(RPC_PROTOCOLTYPE).toInt();
	ui->rpc_protocolType->setCurrentIndex(0);
	for (int i = 0; i < ui->rpc_protocolType->count(); ++i) {
		if(ui->rpc_protocolType->itemData(i).toInt() == proto_type) {
			ui->rpc_protocolType->setCurrentIndex(i);
			break;
		}
	}
	ui->chkMuteHeartBeats->setChecked(props.value(MUTEHEARTBEATS).toBool());
	ui->shvRoot->setText(props.value(SHVROOT).toString());
}

void DlgBrokerProperties::done(int res)
{
	QSettings settings;
	settings.setValue(QStringLiteral("ui/dlgServerProperties/geometry"), saveGeometry());
	Super::done(res);
}
