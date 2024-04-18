#ifndef DLGSUBSCRIPTIONPARAMETERS_H
#define DLGSUBSCRIPTIONPARAMETERS_H

#include <QDialog>

//namespace qfopcua { struct SubscriptionParameters; }

namespace Ui {
class DlgSubscriptionParameters;
}

class DlgSubscriptionParameters : public QDialog
{
	Q_OBJECT

public:
	explicit DlgSubscriptionParameters(QWidget *parent = nullptr);
	~DlgSubscriptionParameters() override;

	//void setSubscriptionParameters(const qfopcua::SubscriptionParameters &params);
	//qfopcua::SubscriptionParameters subscriptionParameters();
private:
	Ui::DlgSubscriptionParameters *ui;
};

#endif // DLGSUBSCRIPTIONPARAMETERS_H
