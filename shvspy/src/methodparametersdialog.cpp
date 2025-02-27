#include "methodparametersdialog.h"
#include "ui_methodparametersdialog.h"

#include <shv/chainpack/rpcvalue.h>
#include <shv/coreqt/log.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QSettings>

namespace cp = shv::chainpack;

namespace {
const int TAB_INDEX_SINGLE_PARAMETER = 0;
const int TAB_INDEX_PARAMETER_MAP = 1;
const int TAB_INDEX_PARAMETER_LIST = 2;
const int TAB_INDEX_CPON = 3;
}

const QVector<cp::RpcValue::Type> MethodParametersDialog::m_supportedTypes {
	cp::RpcValue::Type::String,
	cp::RpcValue::Type::Int,
	cp::RpcValue::Type::UInt,
	cp::RpcValue::Type::Double,
	cp::RpcValue::Type::Bool,
	cp::RpcValue::Type::DateTime,
};

MethodParametersDialog::MethodParametersDialog(const QString &path, const QString &method, const cp::RpcValue &params, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::MethodParametersDialog)
	, m_syntaxCheckTimer(this)
	, m_path(path)
	, m_method(method)
	, m_currentTabIndex(TAB_INDEX_CPON)
{
	shvLogFuncFrame() << "method:" << method << "params:" << params.toCpon();
	ui->setupUi(this);

	ui->parsingSingleLabel->hide();

	ui->removeListButton->setEnabled(false);
	ui->parsingListLabel->hide();

	ui->removeMapButton->setEnabled(false);
	ui->parsingMapLabel->hide();

	connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MethodParametersDialog::onCurrentTabChanged);
	connect(ui->addListButton, &QPushButton::clicked, this, QOverload<>::of(&MethodParametersDialog::newListParameter));
	connect(ui->addMapButton, &QPushButton::clicked, this, QOverload<>::of(&MethodParametersDialog::newMapParameter));
	connect(ui->removeListButton, &QPushButton::clicked, this, &MethodParametersDialog::removeListParameter);
	connect(ui->removeMapButton, &QPushButton::clicked, this, &MethodParametersDialog::removeMapParameter);
	connect(ui->clearButton, &QPushButton::clicked, this, &MethodParametersDialog::clear);
	connect(ui->parameterListTable, &QTableWidget::currentCellChanged, this, &MethodParametersDialog::onListCurrentCellChanged);
	connect(ui->parameterMapTable, &QTableWidget::currentCellChanged, this, &MethodParametersDialog::onMapCurrentCellChanged);
	connect(ui->rawCponEdit, &QPlainTextEdit::textChanged, &m_syntaxCheckTimer, QOverload<>::of(&QTimer::start));
	connect(ui->rawCponEdit, &QPlainTextEdit::textChanged, this, [this]() {
		m_cponEdited = true;
	});

	ui->singleParameterTable->setRowCount(1);

	m_singleTypeCombo = new QComboBox(this);
	ui->singleParameterTable->setCellWidget(0, 0, m_singleTypeCombo);
	for (cp::RpcValue::Type t : m_supportedTypes) {
		m_singleTypeCombo->addItem(cp::RpcValue::typeToName(t), static_cast<int>(t));
	}
	m_singleValueGetters << ValueGetter();
	m_singleValueSetters << ValueSetter();
	switchToString(ui->singleParameterTable, 0, 1, m_singleValueGetters, m_singleValueSetters);

	connect(m_singleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MethodParametersDialog::onSingleTypeChanged);

	if (params.isValid()) {
		loadParams(params);
	}
	m_syntaxCheckTimer.setInterval(500);
	m_syntaxCheckTimer.setSingleShot(true);
	connect(&m_syntaxCheckTimer, &QTimer::timeout, this, &MethodParametersDialog::checkSyntax);

	QSettings settings;
	restoreGeometry(settings.value(QStringLiteral("ui/MethodParametersDialog/geometry")).toByteArray());
}

MethodParametersDialog::~MethodParametersDialog()
{
	QSettings settings;
	settings.setValue(QStringLiteral("ui/MethodParametersDialog/geometry"), saveGeometry());
	delete ui;
}

cp::RpcValue MethodParametersDialog::value() const
{
	if (ui->tabWidget->currentIndex() == TAB_INDEX_SINGLE_PARAMETER) {
		return singleParamValue();
	}
	if (ui->tabWidget->currentIndex() == TAB_INDEX_PARAMETER_MAP) {
		return mapParamValue();
	}
	if (ui->tabWidget->currentIndex() == TAB_INDEX_PARAMETER_LIST) {
		return listParamValue();
	}
	if (ui->tabWidget->currentIndex() == TAB_INDEX_CPON) {
		std::string cpon = ui->rawCponEdit->toPlainText().toStdString();
		if (!cpon.empty()) {
			std::string err;
			cp::RpcValue val = cp::RpcValue::fromCpon(ui->rawCponEdit->toPlainText().toStdString(), &err);
			if (err.empty()) {
				return val;
			}
		}
	}
	return cp::RpcValue();
}

void MethodParametersDialog::newSingleParameter(const shv::chainpack::RpcValue &param)
{
	cp::RpcValue::Type current_type = static_cast<cp::RpcValue::Type>(m_singleTypeCombo->itemData(m_singleTypeCombo->currentIndex()).toInt());
	cp::RpcValue::Type requested_type;
	if (param.isValid()) {
		requested_type = param.type();
	}
	else {
		requested_type = cp::RpcValue::Type::String;
	}
	if (current_type != requested_type) {
		disconnect(m_singleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MethodParametersDialog::onSingleTypeChanged);
		auto index = static_cast<int>(m_supportedTypes.indexOf(requested_type));
		m_singleTypeCombo->setCurrentIndex(index);
		switchByType(requested_type, ui->singleParameterTable, 0, 1, m_singleValueGetters, m_singleValueSetters);
		connect(m_singleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MethodParametersDialog::onSingleTypeChanged);
	}
	m_singleValueSetters[0](param);
}

void MethodParametersDialog::newListParameter()
{
	newListParameter(cp::RpcValue());
}

void MethodParametersDialog::newMapParameter()
{
	newMapParameter("", cp::RpcValue());
}

void MethodParametersDialog::newMapParameter(const QString &key, const cp::RpcValue &param)
{
	int row = ui->parameterMapTable->rowCount();
	ui->parameterMapTable->setRowCount(row + 1);

	ui->parameterMapTable->setItem(row, 0, new QTableWidgetItem(key));
	auto *combo = new QComboBox(this);
	ui->parameterMapTable->setCellWidget(row, 1, combo);
	for (cp::RpcValue::Type t : m_supportedTypes) {
		combo->addItem(cp::RpcValue::typeToName(t), static_cast<int>(t));
	}
	m_mapValueGetters << ValueGetter();
	m_mapValueSetters << ValueSetter();
	ui->parameterMapTable->setVerticalHeaderItem(row, new QTableWidgetItem("  "));

	if (param.isValid()) {
		cp::RpcValue::Type t = param.type();
		auto index = static_cast<int>(m_supportedTypes.indexOf(t));
		combo->setCurrentIndex(index);
		switchByType(t, ui->parameterMapTable, row, 2, m_mapValueGetters, m_mapValueSetters);
		m_mapValueSetters[row](param);
	}
	else {
		int index = static_cast<int>(m_supportedTypes.indexOf(cp::RpcValue::Type::String));
		combo->setCurrentIndex(index);
		switchToString(ui->parameterMapTable, row, 2, m_mapValueGetters, m_mapValueSetters);
	}
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, combo, row](int index) {
		auto t = static_cast<cp::RpcValue::Type>(combo->itemData(index).toInt());
		switchByType(t, ui->parameterMapTable, row, 2, m_mapValueGetters, m_mapValueSetters);
	});
}

void MethodParametersDialog::newListParameter(const cp::RpcValue &param)
{
	int row = ui->parameterListTable->rowCount();
	ui->parameterListTable->setRowCount(row + 1);

	auto *combo = new QComboBox(this);
	ui->parameterListTable->setCellWidget(row, 0, combo);
	for (cp::RpcValue::Type t : m_supportedTypes) {
		combo->addItem(cp::RpcValue::typeToName(t), static_cast<int>(t));
	}
	m_listValueGetters << ValueGetter();
	m_listValueSetters << ValueSetter();
	ui->parameterListTable->setVerticalHeaderItem(row, new QTableWidgetItem("  "));

	if (param.isValid()) {
		cp::RpcValue::Type t = param.type();
		auto index = static_cast<int>(m_supportedTypes.indexOf(t));
		combo->setCurrentIndex(index);
		switchByType(t, ui->parameterListTable, row, 1, m_listValueGetters, m_listValueSetters);
		m_listValueSetters[row](param);
	}
	else {
		auto index = static_cast<int>(m_supportedTypes.indexOf(cp::RpcValue::Type::String));
		combo->setCurrentIndex(index);
		switchToString(ui->parameterListTable, row, 1, m_listValueGetters, m_listValueSetters);
	}
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, combo, row](int index) {
		auto t = static_cast<cp::RpcValue::Type>(combo->itemData(index).toInt());
		switchByType(t, ui->parameterListTable, row, 1, m_listValueGetters, m_listValueSetters);
	});
}

void MethodParametersDialog::onListCurrentCellChanged(int row, int col)
{
	Q_UNUSED(col);
	ui->removeListButton->setEnabled(row != -1);
}

void MethodParametersDialog::onMapCurrentCellChanged(int row, int col)
{
	Q_UNUSED(col);
	ui->removeMapButton->setEnabled(row != -1);
}

void MethodParametersDialog::onSingleTypeChanged(int type)
{
	auto t = static_cast<cp::RpcValue::Type>(m_singleTypeCombo->itemData(type).toInt());
	switchByType(t, ui->singleParameterTable, 0, 1, m_singleValueGetters, m_singleValueSetters);
}

void MethodParametersDialog::removeListParameter()
{
	int row = ui->parameterListTable->currentRow();
	ui->parameterListTable->removeRow(row);
	m_listValueGetters.removeAt(row);
	m_listValueSetters.removeAt(row);
}

void MethodParametersDialog::removeMapParameter()
{
	int row = ui->parameterMapTable->currentRow();
	ui->parameterMapTable->removeRow(row);
	m_mapValueGetters.removeAt(row);
	m_mapValueSetters.removeAt(row);
}

bool MethodParametersDialog::tryParseSingleParam(const cp::RpcValue &params)
{
	if (!m_supportedTypes.contains(params.type())) {
		return false;
	}
	newSingleParameter(params);
	return true;
}

bool MethodParametersDialog::tryParseListParams(const cp::RpcValue &params)
{
	if (!params.isList()) {
		return false;
	}
	const auto &param_list = params.asList();

	for (const cp::RpcValue &param : param_list) {
		if (!m_supportedTypes.contains(param.type())) {
			return false;
		}
	}
	for (const cp::RpcValue &param : param_list) {
		newListParameter(param);
	}
	return true;
}

bool MethodParametersDialog::tryParseMapParams(const cp::RpcValue &params)
{
	if (!params.isMap()) {
		return false;
	}
	const auto &param_map = params.asMap();

	for (const auto &param_pair : param_map) {
		if (!m_supportedTypes.contains(param_pair.second.type())) {
			return false;
		}
	}
	for (const auto &param_pair : param_map) {
		newMapParameter(QString::fromStdString(param_pair.first), param_pair.second);
	}
	return true;
}

void MethodParametersDialog::switchToBool(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *checkbox = new QCheckBox(this);
	table->setCellWidget(row, col, checkbox);
	getters[row] = [checkbox]() {
		return cp::RpcValue(checkbox->isChecked() ? true : false);
	};
	setters[row] = [checkbox](const cp::RpcValue &param) {
		checkbox->setChecked(param.toBool());
	};
}

void MethodParametersDialog::switchToInt(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *line_edit = new QLineEdit(this);
	table->setCellWidget(row, col, line_edit);
	line_edit->setValidator(new QIntValidator(line_edit));
	getters[row] = [line_edit]() {
		return cp::RpcValue(line_edit->text().toInt());
	};
	setters[row] = [line_edit](const cp::RpcValue &param) {
		line_edit->setText(QString::number(param.toInt()));
	};
}

void MethodParametersDialog::switchToUInt(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *line_edit = new QLineEdit(this);
	table->setCellWidget(row, col, line_edit);
	line_edit->setValidator(new QIntValidator(0, std::numeric_limits<int>::max(), line_edit));
	getters[row] = [line_edit]() {
		return cp::RpcValue(line_edit->text().toUInt());
	};
	setters[row] = [line_edit](const cp::RpcValue &param) {
		line_edit->setText(QString::number(param.toUInt()));
	};
}

void MethodParametersDialog::switchToString(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *line_edit = new QLineEdit(this);
	line_edit->setMaxLength(std::numeric_limits<int>::max());
	table->setCellWidget(row, col, line_edit);
	getters[row] = [line_edit]() {
		return  cp::RpcValue(line_edit->text().toStdString());
	};
	setters[row] = [line_edit](const cp::RpcValue &param) {
		line_edit->setText(QString::fromStdString(param.asString()));
	};
}

void MethodParametersDialog::switchToDouble(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *line_edit = new QLineEdit(this);
	table->setCellWidget(row, col, line_edit);
	auto *v = new QDoubleValidator(line_edit);
	v->setLocale(QLocale::C);
	line_edit->setValidator(v);
	getters[row] = [line_edit]() {
		return cp::RpcValue(line_edit->text().toDouble());
	};
	setters[row] = [line_edit](const cp::RpcValue &param) {
		line_edit->setText(QString::number(param.toDouble()));
	};
}

void MethodParametersDialog::switchToDateTime(QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	auto *datetime_widget = new QWidget(this);
	auto *edit = new QDateTimeEdit(datetime_widget);
#if !defined(__EMSCRIPTEN__)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	edit->setTimeZone(QTimeZone::utc());
#else
	edit->setTimeSpec(Qt::TimeSpec::UTC);
#endif
#endif
	auto *today = new QPushButton("...", datetime_widget);
	today->setFixedWidth(20);
	auto *layout = new QHBoxLayout;
	layout->setContentsMargins(0,0,0,0);
	layout->setSpacing(0);
	layout->addWidget(edit);
	layout->addWidget(today);
	datetime_widget->setLayout(layout);
	edit->setDisplayFormat("dd.MM.yyyy HH:mm:ss");
	edit->setCalendarPopup(true);
	table->setCellWidget(row, col, datetime_widget);
	connect(today, &QPushButton::clicked, edit, [edit]() {
		edit->setDateTime(QDateTime::currentDateTime());
	});
	getters[row] = [edit]() {
		return cp::RpcValue::DateTime::fromMSecsSinceEpoch(edit->dateTime().toMSecsSinceEpoch());
	};
	setters[row] = [edit](const cp::RpcValue &param) {
		edit->setDateTime(QDateTime::fromMSecsSinceEpoch(param.toDateTime().msecsSinceEpoch()));
	};
}

void MethodParametersDialog::switchByType(const cp::RpcValue::Type &type, QTableWidget *table, int row, int col, QVector<ValueGetter> &getters, QVector<ValueSetter> &setters)
{
	switch (type) {
	case cp::RpcValue::Type::Int:
		switchToInt(table, row, col, getters, setters);
		break;
	case cp::RpcValue::Type::UInt:
		switchToUInt(table, row, col, getters, setters);
		break;
	case cp::RpcValue::Type::Bool:
		switchToBool(table, row, col, getters, setters);
		break;
	case cp::RpcValue::Type::String:
		switchToString(table, row, col, getters, setters);
		break;
	case cp::RpcValue::Type::Double:
		switchToDouble(table, row, col, getters, setters);
		break;
	case cp::RpcValue::Type::DateTime:
		switchToDateTime(table, row, col, getters, setters);
		break;
	default:
		break;
	}
}

void MethodParametersDialog::switchToCpon()
{
	cp::RpcValue value;
	if (m_currentTabIndex == TAB_INDEX_PARAMETER_LIST) {
		if (!ui->parameterListTable->isHidden()) {
			value = listParamValue();
			if (value.isValid()) {
				ui->rawCponEdit->setPlainText(QString::fromStdString(value.toPrettyString("    ")));
			}
			else {
				ui->rawCponEdit->setPlainText(QString());
			}
			m_cponEdited = false;
		}
	}
	else if (m_currentTabIndex == TAB_INDEX_PARAMETER_MAP) {
		if (!ui->parameterMapTable->isHidden()) {
			value = mapParamValue();
			if (value.isValid()) {
				ui->rawCponEdit->setPlainText(QString::fromStdString(value.toPrettyString("    ")));
			}
			else {
				ui->rawCponEdit->setPlainText(QString());
			}
			m_cponEdited = false;
		}
	}
	else if (m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER) {
		if (!ui->singleParameterTable->isHidden()) {
			value = singleParamValue();
			if (value.isValid()) {
				ui->rawCponEdit->setPlainText(QString::fromStdString(value.toPrettyString("    ")));
			}
			else {
				ui->rawCponEdit->setPlainText(QString());
			}
			m_cponEdited = false;
		}
	}
}

void MethodParametersDialog::switchToMap()
{
	if (m_currentTabIndex == TAB_INDEX_CPON || m_cponEdited ||
		(m_currentTabIndex == TAB_INDEX_PARAMETER_LIST && !ui->parameterListTable->isHidden()) ||
		(m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER && !ui->singleParameterTable->isHidden())) {
		clearParamMap();
		bool parsed = false;
		if (m_currentTabIndex == TAB_INDEX_CPON ||
			(m_currentTabIndex == TAB_INDEX_PARAMETER_LIST && ui->parameterListTable->isHidden() && m_cponEdited) ||
			(m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER && ui->singleParameterTable->isHidden() && m_cponEdited)) {
			std::string cpon = ui->rawCponEdit->toPlainText().toStdString();
			if (cpon.empty()) {
				parsed = true;
			}
			else {
				std::string err;
				cp::RpcValue val = cp::RpcValue::fromCpon(cpon, &err);
				if (err.empty()) {
					parsed = tryParseMapParams(val);
				}
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_PARAMETER_LIST) {
			if (!ui->parameterListTable->isHidden() && ui->parameterListTable->rowCount() == 0) {
				parsed = true;
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER) {
			if (!ui->singleParameterTable->isHidden() && !singleParamValue().isValid()) {
				parsed = true;
			}
		}
		if (!parsed) {
			ui->addMapButton->setEnabled(false);
			ui->parameterMapTable->hide();
			ui->parsingMapLabel->show();
		}
	}
}

void MethodParametersDialog::switchToList()
{
	if (m_currentTabIndex == TAB_INDEX_CPON || m_cponEdited ||
		(m_currentTabIndex == TAB_INDEX_PARAMETER_MAP && !ui->parameterMapTable->isHidden()) ||
		(m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER && !ui->singleParameterTable->isHidden())) {
		clearParamList();
		bool parsed = false;
		if (m_currentTabIndex == TAB_INDEX_CPON ||
			(m_currentTabIndex == TAB_INDEX_PARAMETER_MAP && ui->parameterMapTable->isHidden() && m_cponEdited) ||
			(m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER && ui->singleParameterTable->isHidden() && m_cponEdited)) {
			std::string cpon = ui->rawCponEdit->toPlainText().toStdString();
			if (cpon.empty()) {
				parsed = true;
			}
			else {
				std::string err;
				cp::RpcValue val = cp::RpcValue::fromCpon(cpon, &err);
				if (err.empty()) {
					parsed = tryParseListParams(val);
				}
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_PARAMETER_MAP) {
			if (!ui->parameterMapTable->isHidden() && ui->parameterMapTable->rowCount() == 0) {
				parsed = true;
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_SINGLE_PARAMETER) {
			if (!ui->singleParameterTable->isHidden() && !singleParamValue().isValid()) {
				parsed = true;
			}
		}
		if (!parsed) {
			ui->addListButton->setEnabled(false);
			ui->parameterListTable->hide();
			ui->parsingListLabel->show();
		}
	}
}

void MethodParametersDialog::switchToSingle()
{
	if (m_currentTabIndex == TAB_INDEX_CPON || m_cponEdited ||
		(m_currentTabIndex == TAB_INDEX_PARAMETER_MAP && !ui->parameterMapTable->isHidden()) ||
		(m_currentTabIndex == TAB_INDEX_PARAMETER_LIST && !ui->parameterListTable->isHidden())) {
		clearSingleParam();
		bool parsed = false;
		if (m_currentTabIndex == TAB_INDEX_CPON ||
			(m_currentTabIndex == TAB_INDEX_PARAMETER_MAP && ui->parameterMapTable->isHidden() && m_cponEdited) ||
			(m_currentTabIndex == TAB_INDEX_PARAMETER_LIST && ui->parameterListTable->isHidden() && m_cponEdited)) {
			std::string cpon = ui->rawCponEdit->toPlainText().toStdString();
			if (cpon.empty()) {
				parsed = true;
			}
			else {
				std::string err;
				cp::RpcValue val = cp::RpcValue::fromCpon(cpon, &err);
				if (err.empty()) {
					parsed = tryParseSingleParam(val);
				}
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_PARAMETER_MAP) {
			if (!ui->parameterMapTable->isHidden() && ui->parameterMapTable->rowCount() == 0) {
				parsed = true;
			}
		}
		else if (m_currentTabIndex == TAB_INDEX_PARAMETER_LIST) {
			if (!ui->parameterListTable->isHidden() && ui->parameterListTable->rowCount() == 0) {
				parsed = true;
			}
		}
		if (!parsed) {
			ui->addListButton->setEnabled(false);
			ui->singleParameterTable->hide();
			ui->parsingSingleLabel->show();
		}
	}
}

void MethodParametersDialog::clearSingleParam()
{
	ui->parsingSingleLabel->hide();
	ui->singleParameterTable->show();

	newSingleParameter(cp::RpcValue());
}

void MethodParametersDialog::clearParamList()
{
	ui->parameterListTable->reset();
	ui->parameterListTable->clearContents();
	ui->parameterListTable->setRowCount(0);
	ui->removeListButton->setEnabled(false);
	ui->addListButton->setEnabled(true);
	ui->parsingListLabel->hide();
	ui->parameterListTable->show();
}

void MethodParametersDialog::clearParamMap()
{
	ui->parameterMapTable->reset();
	ui->parameterMapTable->clearContents();
	ui->parameterMapTable->setRowCount(0);
	ui->removeMapButton->setEnabled(false);
	ui->addMapButton->setEnabled(true);
	ui->parsingMapLabel->hide();
	ui->parameterMapTable->show();
}

void MethodParametersDialog::clear()
{
	clearSingleParam();
	clearParamMap();
	clearParamList();
	ui->rawCponEdit->clear();
}

void MethodParametersDialog::onCurrentTabChanged(int index)
{
	if (index == TAB_INDEX_CPON) {
		switchToCpon();
	}
	else if (index == TAB_INDEX_PARAMETER_LIST) {
		switchToList();
	}
	else if (index == TAB_INDEX_PARAMETER_MAP) {
		switchToMap();
	}
	else if (index == TAB_INDEX_SINGLE_PARAMETER) {
		switchToSingle();
	}
	m_currentTabIndex = index;
}

void MethodParametersDialog::checkSyntax()
{
	std::string cpon = ui->rawCponEdit->toPlainText().toStdString();
	if (!cpon.empty()) {
		std::string err;
		cp::RpcValue val = cp::RpcValue::fromCpon(ui->rawCponEdit->toPlainText().toStdString(), &err);
		if (!err.empty()) {
			QPalette pal = ui->rawCponEdit->palette();
			pal.setColor(QPalette::ColorRole::Text, Qt::red);
			ui->rawCponEdit->setPalette(pal);
			return;
		}
	}
	QPalette pal = ui->rawCponEdit->palette();
	pal.setColor(QPalette::ColorRole::Text, Qt::black);
	ui->rawCponEdit->setPalette(pal);
}

void MethodParametersDialog::loadParams(const QString &s)
{
	cp::RpcValue params = cp::RpcValue::fromCpon(s.toStdString());
	loadParams(params);
}

void MethodParametersDialog::loadParams(const shv::chainpack::RpcValue &params)
{
	disconnect(ui->tabWidget, &QTabWidget::currentChanged, this, &MethodParametersDialog::onCurrentTabChanged);

	clear();
	if (params.isValid()) {
		ui->tabWidget->setCurrentIndex(TAB_INDEX_SINGLE_PARAMETER);
		if (!tryParseSingleParam(params)) {
			ui->tabWidget->setCurrentIndex(TAB_INDEX_PARAMETER_MAP);
			if (!tryParseMapParams(params)) {
				ui->tabWidget->setCurrentIndex(TAB_INDEX_PARAMETER_LIST);
				if (!tryParseListParams(params)) {
					ui->tabWidget->setCurrentIndex(TAB_INDEX_CPON);
					ui->rawCponEdit->setPlainText(QString::fromStdString(params.toPrettyString("    ")));
				}
			}
		}
	}
	connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MethodParametersDialog::onCurrentTabChanged);
}

cp::RpcValue MethodParametersDialog::singleParamValue() const
{
	if (!ui->parsingSingleLabel->isHidden()) {
		return cp::RpcValue();
	}

	cp::RpcValue val = m_singleValueGetters[0]();
	if (val.isString() && val.asString().empty()) {
		return cp::RpcValue();
	}
	return val;
}

cp::RpcValue MethodParametersDialog::listParamValue() const
{
	if (ui->parameterListTable->rowCount() == 0) {
		return cp::RpcValue();
	}
	cp::RpcValue::List list;
	for (int i = 0; i < ui->parameterListTable->rowCount(); ++i) {
		list.push_back(m_listValueGetters[i]());
	}
	return list;
}

cp::RpcValue MethodParametersDialog::mapParamValue() const
{
	if (ui->parameterMapTable->rowCount() == 0) {
		return cp::RpcValue();
	}
	cp::RpcValue::Map map;
	for (int i = 0; i < ui->parameterMapTable->rowCount(); ++i) {
		map[ui->parameterMapTable->item(i, 0)->text().toStdString()] = m_mapValueGetters[i]();
	}
	return map;
}
