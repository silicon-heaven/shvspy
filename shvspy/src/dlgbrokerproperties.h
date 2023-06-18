#ifndef DLGBROKERPROPERTIES_H
#define DLGBROKERPROPERTIES_H

#include <QDialog>
#include <QVariantMap>

namespace Ui {
class DlgBrokerProperties;
}

class DlgBrokerProperties : public QDialog
{
	Q_OBJECT
private:
	typedef QDialog Super;
public:
	explicit DlgBrokerProperties(QWidget *parent = nullptr);
	~DlgBrokerProperties() Q_DECL_OVERRIDE;

	QVariantMap brokerProperties() const;
	void setBrokerProperties(const QVariantMap &props);

	void done(int res) Q_DECL_OVERRIDE;

private:
	Ui::DlgBrokerProperties *ui;
	QVariantList m_subscriptions;
};

#endif // DLGBROKERPROPERTIES_H
