#include "subscriptionswidget.h"
#include "ui_subscriptionswidget.h"

#include "subscriptionsmodel/subscriptionstableitemdelegate.h"

#include "theapp.h"

#include <shv/coreqt/log.h>

#include <QMessageBox>

SubscriptionsWidget::SubscriptionsWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::SubscriptionsWidget)
{
	ui->setupUi(this);

	ui->tvSubscriptions->setModel(&m_subscriptionsModel);
	ui->tvSubscriptions->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
	ui->tvSubscriptions->verticalHeader()->setDefaultSectionSize(static_cast<int>(fontMetrics().height() * 1.3));
	ui->tvSubscriptions->setItemDelegateForColumn(SubscriptionsModel::Columns::ColMethod, new SubscriptionsTableItemDelegate(this));
}

SubscriptionsWidget::~SubscriptionsWidget()
{
	delete ui;
}

void SubscriptionsWidget::onBrokerConnectedChanged(int broker_id, bool is_connected)
{
	m_subscriptionsModel.onBrokerConnectedChanged(broker_id, is_connected);
	ui->tvSubscriptions->resizeColumnsToContents();
}

void SubscriptionsWidget::onSubscriptionAdded(int broker_id, const std::string &shv_path, const std::string &method)
{
	ShvBrokerNodeItem *nd = TheApp::instance()->serverTreeModel()->brokerById(broker_id);

	if (nd == nullptr){
		return;
	}

	SubscriptionsModel::Subscription sub(broker_id, QString::fromStdString(nd->nodeId()));
	sub.setShvPath(QString::fromStdString(shv_path));
	sub.setMethod(QString::fromStdString(method));
	sub.setIsEnabled(true);

	m_subscriptionsModel.addSubscription(sub);
	ui->tvSubscriptions->resizeColumnsToContents();
}
