#pragma once

#include <QDialog>

#include <QTimer>
#include <functional>

#include <shv/chainpack/rpcvalue.h>

namespace Ui {
class MethodParametersDialog;
}

class QComboBox;
class QTableWidget;
class LastUsedParamsWidget;

class MethodParametersDialog : public QDialog
{
	Q_OBJECT

public:
	explicit MethodParametersDialog(const QString &path, const QString &method, const shv::chainpack::RpcValue &params, QWidget *parent = nullptr);
	~MethodParametersDialog() override;

	shv::chainpack::RpcValue value() const;

private:
	using ValueGetter = std::function<shv::chainpack::RpcValue()>;
	using ValueSetter = std::function<void(const shv::chainpack::RpcValue &param)>;

	void newSingleParameter(const shv::chainpack::RpcValue &param);
	void newListParameter();
	void newListParameter(const shv::chainpack::RpcValue &param);
	void newMapParameter();
	void newMapParameter(const QString &key, const shv::chainpack::RpcValue &param);
	void onListCurrentCellChanged(int row, int col);
	void onMapCurrentCellChanged(int row, int col);
	void onSingleTypeChanged(int type);
	void removeListParameter();
	void removeMapParameter();
	bool tryParseSingleParam(const shv::chainpack::RpcValue &params);
	bool tryParseListParams(const shv::chainpack::RpcValue &params);
	bool tryParseMapParams(const shv::chainpack::RpcValue &params);

	void switchToBool(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);
	void switchToInt(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);
	void switchToUInt(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);
	void switchToString(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);
	void switchToDouble(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);
	void switchToDateTime(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);

	void switchByType(const shv::chainpack::RpcValue::Type &type, QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters);

	void switchToCpon();
	void switchToMap();
	void switchToList();
	void switchToSingle();

	void clearSingleParam();
	void clearParamList();
	void clearParamMap();
	void clear();
	void onCurrentTabChanged(int index);
	void checkSyntax();
	void loadParams(const QString &s);
	void loadParams(const shv::chainpack::RpcValue &params);

	shv::chainpack::RpcValue singleParamValue() const;
	shv::chainpack::RpcValue listParamValue() const;
	shv::chainpack::RpcValue mapParamValue() const;

	Ui::MethodParametersDialog *ui;
	QComboBox *m_singleTypeCombo;
	QVector<ValueGetter> m_singleValueGetters;
	QVector<ValueSetter> m_singleValueSetters;
	QVector<ValueGetter> m_listValueGetters;
	QVector<ValueSetter> m_listValueSetters;
	QVector<ValueGetter> m_mapValueGetters;
	QVector<ValueSetter> m_mapValueSetters;
	static QVector<shv::chainpack::RpcValue::Type> m_supportedTypes;
	QTimer m_syntaxCheckTimer;
	QString m_path;
	QString m_method;
	LastUsedParamsWidget *m_usedParamsWidget = nullptr;
	int m_currentTabIndex;
	bool m_cponEdited = false;
};
