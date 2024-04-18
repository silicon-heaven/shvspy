#ifndef SUBSCRIPTIONSWIDGET_H
#define SUBSCRIPTIONSWIDGET_H

#include <shv/iotqt/rpc/deviceconnection.h>

#include "servertreemodel/servertreemodel.h"
#include "servertreemodel/shvbrokernodeitem.h"
#include "subscriptionsmodel/subscriptionsmodel.h"

#include <QWidget>
#include <QTableWidgetItem>

namespace Ui {
class SubscriptionsWidget;
}

class SubscriptionsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit SubscriptionsWidget(QWidget *parent = nullptr);
	~SubscriptionsWidget() override;

	SubscriptionsModel &subscriptionsModel();
	void onBrokerConnectedChanged(int broker_id, bool is_connected);
	void onSubscriptionAdded(int broker_id, const std::string &shv_path, const std::string &method);
private:
	Ui::SubscriptionsWidget *ui;
	SubscriptionsModel m_subscriptionsModel;

};

#endif // SUBSCRIPTIONSWIDGET_H
