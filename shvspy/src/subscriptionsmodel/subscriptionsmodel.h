#pragma once

#include <shv/chainpack/rpcvalue.h>

#include "../servertreemodel/shvbrokernodeitem.h"

#include <QAbstractTableModel>
#include <QMetaEnum>

namespace shv::chainpack { class RpcMessage; }

class SubscriptionsModel : public QAbstractTableModel
{
	Q_OBJECT
private:
	typedef QAbstractTableModel Super;
public:
	enum Columns {ColServer = 0, ColPath, ColMethod, ColSource, ColPermanent, ColEnabled, ColCount};

	class Subscription{
	public:
		Subscription();
		Subscription(int broker_id, const QString &server_name);

		void setConfig(const QVariantMap &config);
		QVariantMap config() const;
		int brokerId() const;
		QString serverName() const;

		QString shvPath() const;
		void setShvPath(const QString &shv_path);
		QString method() const;
		void setMethod(const QString &method);
		QString source() const;
		void setSource(const QString &source);
		bool isPermanent() const;
		void setIsPermanent(bool val);
		bool isEnabled() const;
		void setIsEnabled(bool val);
	private:
		int m_brokerId;
		QString m_serverName;
		QVariantMap m_config;
	};

public:
	SubscriptionsModel(QObject *parent = nullptr);
	~SubscriptionsModel() Q_DECL_OVERRIDE;
public:
	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent) const override;
	Qt::ItemFlags flags(const QModelIndex &ix) const Q_DECL_OVERRIDE;
	QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;
	bool setData(const QModelIndex &ix, const QVariant &val, int role = Qt::EditRole) Q_DECL_OVERRIDE;
	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const Q_DECL_OVERRIDE;
	void reload();

	void onBrokerConnectedChanged(int broker_id, bool is_connected);
	void addSubscription(Subscription sub);

	Q_SIGNAL void subscriptionEnabled(int broker_id, const std::string &shv_path, const std::string &method);
private:
	int subscriptionIndex(int broker_id, const QString &shv_path, const QString &method);
	QVariantList brokerSubscriptions(int broker_id);

	QVector<Subscription> m_subscriptions;
};
