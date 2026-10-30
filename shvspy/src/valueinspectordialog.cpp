#include "valueinspectordialog.h"
#include "ui_texteditdialog.h"

#include "typeinfoloader.h"
#include "theapp.h"
#include "attributesmodel/attributesmodel.h"

#include <shv/chainpack/rpc.h>
#include <shv/core/utils/shvpath.h>
#include <shv/core/utils/shvtypeinfo.h>
#include <shv/coreqt/rpc.h>

#include <QTreeWidgetItem>

#include <set>

namespace cp = shv::chainpack;
using shv::core::utils::ShvFieldDescr;
using shv::core::utils::ShvTypeDescr;
using shv::core::utils::ShvTypeInfo;

namespace {
enum TreeColumn { ColName = 0, ColValue, ColType, ColUnit, ColDescription };

// string and blob are shown as they are, type info can only describe them
bool is_decodable(const cp::RpcValue &value)
{
	return !value.isString() && !value.isBlob();
}

struct DecodedValue
{
	QString name;
	QString value;
	QString typeName;
	QString unit;
	QString description;
	QList<DecodedValue> children;
};

// description is often the same as label in type info, it is shown once then
QString description_text(const ShvTypeDescr &descr)
{
	auto label = QString::fromStdString(descr.label());
	auto description = QString::fromStdString(descr.description());
	if (description.isEmpty() || description == label) {
		return label;
	}
	return QStringLiteral("%1 (%2)").arg(label, description);
}

QString container_summary(const cp::RpcValue &value)
{
	if (value.isList()) {
		return QStringLiteral("List(%1)").arg(value.asList().size());
	}
	if (value.isMap()) {
		return QStringLiteral("Map(%1)").arg(value.asMap().size());
	}
	if (value.isIMap()) {
		return QStringLiteral("IMap(%1)").arg(value.asIMap().size());
	}
	return QString::fromStdString(value.toCpon());
}

QString format_number(const cp::RpcValue &value, const ShvTypeDescr &type_descr)
{
	if (auto dec_places = type_descr.decimalPlaces(); dec_places > 0) {
		return QString::number(value.toDouble(), 'f', dec_places);
	}
	return QString::fromStdString(value.toCpon());
}

DecodedValue decode_value(const ShvTypeInfo &type_info, const QString &name, const cp::RpcValue &value, const ShvFieldDescr &field_descr, const ShvTypeDescr &type_descr);

void decode_raw_children(DecodedValue &ret, const ShvTypeInfo &type_info, const cp::RpcValue &value)
{
	if (value.isList()) {
		int i = 0;
		for (const auto &item : value.asList()) {
			ret.children << decode_value(type_info, QStringLiteral("[%1]").arg(i++), item, {}, {});
		}
	}
	else if (value.isMap()) {
		for (const auto &[key, val] : value.asMap()) {
			ret.children << decode_value(type_info, QString::fromStdString(key), val, {}, {});
		}
	}
	else if (value.isIMap()) {
		for (const auto &[key, val] : value.asIMap()) {
			ret.children << decode_value(type_info, QString::number(key), val, {}, {});
		}
	}
}

DecodedValue decode_value(const ShvTypeInfo &type_info, const QString &name, const cp::RpcValue &value, const ShvFieldDescr &field_descr, const ShvTypeDescr &type_descr)
{
	using Type = ShvTypeDescr::Type;

	DecodedValue ret;
	ret.name = name;
	ret.typeName = QString::fromStdString(field_descr.typeName());
	if (ret.typeName.isEmpty()) {
		ret.typeName = (type_descr.type() == Type::Invalid) ? cp::RpcValue::typeToName(value.type()) : QString::fromStdString(type_descr.typeName());
	}
	ret.unit = QString::fromStdString(field_descr.unit().empty() ? type_descr.unit() : field_descr.unit());
	ret.description = description_text(field_descr);
	if (ret.description.isEmpty()) {
		ret.description = description_text(type_descr);
	}

	if (value.isValueNotAvailable()) {
		ret.value = QStringLiteral("n/a");
		return ret;
	}

	switch (type_descr.type()) {
	case Type::BitField: {
		auto uval = static_cast<qulonglong>(value.toUInt64());
		ret.value = QStringLiteral("0x%1 (%2)").arg(uval, 0, 16).arg(uval);
		for (const auto &fld : type_descr.fields()) {
			if (fld.name().empty()) {
				continue;
			}
			ret.children << decode_value(type_info, QString::fromStdString(fld.name()), fld.bitfieldValue(uval), fld, type_info.findTypeDescription(fld.typeName()));
		}
		break;
	}
	case Type::Enum: {
		ret.value = QString::fromStdString(value.toCpon());
		for (const auto &fld : type_descr.fields()) {
			if (fld.bitRange().toInt() == value.toInt()) {
				auto name = QString::fromStdString(fld.name());
				auto label = fld.label().empty()? name: QString::fromStdString(fld.label());
				ret.value = QStringLiteral("%1 (%2)").arg(label, QString::number(value.toInt()));
				// enum item label is shown as value already, so its description is added only when it says more
				if (auto descr = QString::fromStdString(fld.description()); !descr.isEmpty() && descr != label && descr != name) {
					ret.description = ret.description.isEmpty()? descr: ret.description + QStringLiteral(": ") + descr;
				}
				break;
			}
		}
		break;
	}
	case Type::Bool:
		ret.value = value.toBool()? QStringLiteral("true"): QStringLiteral("false");
		break;
	case Type::UInt:
		ret.value = QString::number(value.toUInt64());
		break;
	case Type::Int:
		ret.value = QString::number(value.toInt64());
		break;
	case Type::Decimal:
	case Type::Double:
		ret.value = format_number(value, type_descr);
		break;
	case Type::String:
		ret.value = value.isString()? QString::fromStdString(value.asString()): QString::fromStdString(value.toCpon());
		break;
	case Type::DateTime:
		ret.value = QString::fromStdString(value.toDateTime().toIsoString());
		break;
	case Type::List: {
		ret.value = container_summary(value);
		auto item_type_name = type_descr.listTypeName();
		auto item_type_descr = type_info.findTypeDescription(item_type_name);
		int i = 0;
		for (const auto &item : value.asList()) {
			ret.children << decode_value(type_info, QStringLiteral("[%1]").arg(i++), item, ShvFieldDescr({}, item_type_name), item_type_descr);
		}
		break;
	}
	case Type::Map: {
		ret.value = container_summary(value);
		const auto &map = value.asMap();
		// fields in type info order first, then the fields not described in type info
		std::set<std::string> described_keys;
		for (const auto &fld : type_descr.fields()) {
			if (map.hasKey(fld.name())) {
				described_keys.insert(fld.name());
				ret.children << decode_value(type_info, QString::fromStdString(fld.name()), map.valref(fld.name()), fld, type_info.findTypeDescription(fld.typeName()));
			}
		}
		for (const auto &[key, val] : map) {
			if (!described_keys.contains(key)) {
				ret.children << decode_value(type_info, QString::fromStdString(key), val, {}, {});
			}
		}
		break;
	}
	case Type::IMap:
	case Type::Invalid:
		ret.value = container_summary(value);
		decode_raw_children(ret, type_info, value);
		break;
	}
	return ret;
}

QTreeWidgetItem *create_tree_item(const DecodedValue &decoded)
{
	auto *item = new QTreeWidgetItem(QStringList{decoded.name, decoded.value, decoded.typeName, decoded.unit, decoded.description});
	item->setToolTip(ColDescription, decoded.description);
	for (const auto &child : decoded.children) {
		item->addChild(create_tree_item(child));
	}
	return item;
}
}

ValueInspectorDialog::ValueInspectorDialog(QWidget *parent)
	: Super(parent)
{
	ui->btDecode->show();
	connect(ui->btDecode, &QPushButton::toggled, this, &ValueInspectorDialog::showTypeInfo);
}

void ValueInspectorDialog::setResult(const QModelIndex &ix)
{
	auto *attributes_model = TheApp::instance()->attributesModel();
	m_value = qvariant_cast<cp::RpcValue>(ix.data(AttributesModel::RpcValueRole));
	if (m_value.isString()) {
		setText(QString::fromStdString(m_value.asString()));
	}
	else if (m_value.isBlob()) {
		const auto &blob = m_value.asBlob();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		auto data = QByteArray::fromRawData(reinterpret_cast<const char*>(blob.data()), blob.size());
#else
		auto data = QByteArrayView(blob).toByteArray();
#endif
		setBlob(data);
	}
	else {
		setText(QString::fromStdString(m_value.toCpon("  ")));
	}
	if (!is_decodable(m_value)) {
		ui->btCompactCpon->setEnabled(false);
		ui->btFormatCpon->setEnabled(false);
		ui->btDecode->setEnabled(false);
	}
	ui->frmTypeInfo->show();
	if (attributes_model->isResultError(ix.row())) {
		setTypeInfoNotAvailable(tr("result is an error"));
		return;
	}
	if (attributes_model->method(ix.row()) != cp::Rpc::METH_GET) {
		setTypeInfoNotAvailable(tr("type info can be applied to the result of method %1 only").arg(cp::Rpc::METH_GET));
		return;
	}
	ui->pbTypeInfo->show();
	ui->lblTypeInfo->setText(tr("Loading type info ..."));
	auto *loader = new TypeInfoLoader(attributes_model->clientConnection(), attributes_model->path(), this);
	connect(loader, &TypeInfoLoader::progress, ui->lblTypeInfo, &QLabel::setText);
	connect(loader, &TypeInfoLoader::finished, this, &ValueInspectorDialog::onTypeInfoLoaded);
	loader->start();
}

void ValueInspectorDialog::setTypeInfoNotAvailable(const QString &reason)
{
	ui->pbTypeInfo->hide();
	ui->lblTypeInfo->setText(tr("Type info is not available: <i>%1</i>").arg(reason.toHtmlEscaped()));
}

void ValueInspectorDialog::onTypeInfoLoaded(const shv::core::utils::ShvTypeInfo &type_info, const QString &property_path, const QString &error)
{
	if (!error.isEmpty()) {
		setTypeInfoNotAvailable(error);
		return;
	}
	std::string field_path;
	ShvFieldDescr field_descr = type_info.propertyDescriptionForPath(property_path.toStdString(), &field_path);
	auto type_descr = type_info.findTypeDescription(field_descr.typeName());
	for (const auto &field_name : shv::core::utils::ShvPath::splitPath(field_path)) {
		field_descr = type_descr.field(std::string(field_name));
		type_descr = type_info.findTypeDescription(field_descr.typeName());
	}
	// property can be found by an empty prefix, but its field path does not lead to any type then
	if (type_descr.type() == ShvTypeDescr::Type::Invalid) {
		setTypeInfoNotAvailable(tr("Type of property %1 not found").arg(property_path));
		return;
	}
	auto decoded = decode_value(type_info, property_path.section('/', -1), m_value, field_descr, type_descr);

	ui->twValue->clear();
	ui->twValue->addTopLevelItem(create_tree_item(decoded));
	ui->twValue->expandAll();
	for (int i = ColName; i < ColDescription; ++i) {
		ui->twValue->resizeColumnToContents(i);
	}

	QString info = tr("Type: <b>%1</b>").arg(decoded.typeName.toHtmlEscaped());
	if (!decoded.description.isEmpty()) {
		info += QStringLiteral(" – <i>") + decoded.description.toHtmlEscaped() + "</i>";
	}
	ui->pbTypeInfo->hide();
	ui->lblTypeInfo->setText(info);
	ui->btDecode->setEnabled(is_decodable(m_value));
}

void ValueInspectorDialog::showTypeInfo(bool on)
{
	const auto *root = ui->twValue->topLevelItem(0);
	// decoded scalar value has no fields, it is shown as text
	bool is_tree = on && root && root->childCount() > 0;
	ui->plainTextEdit->setVisible(!is_tree);
	ui->twValue->setVisible(is_tree);
	if (!on) {
		// Compact and Format buttons format the raw text then
		setText(QString::fromStdString(m_value.toCpon("  ")));
	}
	else if (root && !is_tree) {
		auto unit = root->text(ColUnit);
		setText(unit.isEmpty() ? root->text(ColValue) : root->text(ColValue) + ' ' + unit);
	}
}
