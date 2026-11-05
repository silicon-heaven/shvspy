#include "rpctypedescription.h"

#include <shv/chainpack/rpcvalue.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <set>
#include <string>

#include <QMap>

namespace cp = shv::chainpack;
using Type = RpcTypeDescription;
using DecodedValue = DecodedValue;

namespace {

const QMap<QString, QString> &standard_types()
{
	static const QMap<QString, QString> types = {
		{QStringLiteral("dir"), QStringLiteral("i{s:name:1,u[b:isGetter:1,b:isSetter,b:largeResult,b:notIndempotent,b:userIDRequired,b:isUpdatable]|n:flags,s|n:paramType,s|n:resultType,i(0,63):accessLevel,{s|n}:signals,{?}:extra:63}")},
		{QStringLiteral("get"), QStringLiteral("i(0,)|n")},
		{QStringLiteral("alert"), QStringLiteral("i{t:date,i(0,63):level,s:id,?:info}")},
		{QStringLiteral("clientInfo"), QStringLiteral("i{i:clientId:1,s|n:userName,s|n:mountPoint,{i|n}|n:subscriptions,{?}:extra:63}")},
		{QStringLiteral("stat"), QStringLiteral("i{i:type,i:size,i:pageSize,t|n:accessTime,t|n:modTime,i|n:maxWrite,i|n:maxRead,i|n:eraseSize}")},
		{QStringLiteral("getLogP"), QStringLiteral("i{t|n:since:1,t|n:until,i(0,)|n:count,s|n:ri}")},
		{QStringLiteral("getLogR"), QStringLiteral("[i{t|n:timestamp:1,i(0,)|n:ref,s|n:path,s|n:signal,s|n:source,?:value,s|n:userId,b|n:repeat,b|n:provisional,b|n:inaccurate,t|n:originalTimestamp}]")},
		{QStringLiteral("getSnapshotP"), QStringLiteral("i{t|n:time:1,s|n:ri}")},
		{QStringLiteral("getSnapshotR"), QStringLiteral("[i{t:timestamp:1,s|n:path:3,s|n:signal,s|n:source,?:value,s|n:userId,b|n:repeat}]")},
		{QStringLiteral("historyRecords"), QStringLiteral("[i{i[normal:1,keep,timeJump,timeAbig]:type,t:timestamp,s|n:path,s|n:signal,s|n:source,?:value,i(0,63)|n:accessLevel,s|n:userId,b|n:repeat,i(0,)|n:id,i(0,)|n:ref,i|n:timeJump:60}]")},
		{QStringLiteral("historySpan"), QStringLiteral("[i(0,):smallest,i(0,):biggest,i(1,):span,i(0,)|n:newestTimeRecord]")},
	};
	return types;
}

std::optional<int64_t> shv_number(const QString &text)
{
	QString value = text;
	bool negative = false;
	if (value.startsWith(QLatin1Char('-'))) {
		negative = true;
		value.remove(0, 1);
	}
	if (value.startsWith(QLatin1Char('^')) || value.startsWith(QLatin1Char('>'))) {
		const QChar prefix = value.front();
		value.remove(0, 1);
		bool ok = false;
		const uint exponent = value.toUInt(&ok);
		if (!ok || exponent >= 63) {
			return {};
		}
		const int64_t number = prefix == QLatin1Char('^') ? (int64_t{1} << exponent) : ((int64_t{1} << exponent) - 1);
		return negative ? -number : number;
	}
	bool ok = false;
	const int64_t number = value.toLongLong(&ok);
	if (!ok) {
		return {};
	}
	return negative ? -number : number;
}

} // namespace

class RpcTypeDescription::Parser
{
public:
	explicit Parser(const QString &source)
		: m_source(source)
	{
	}

	std::optional<Type> parse(QString *error)
	{
		auto type = parseOneOf();
		if (type && m_pos != m_source.size()) {
			fail("unexpected character");
			type.reset();
		}
		if (!type && error) {
			*error = m_error;
		}
		return type;
	}

private:
	QChar peek() const { return m_pos < m_source.size() ? m_source.at(static_cast<int>(m_pos)) : QChar{}; }
	bool atEnd() const { return m_pos >= m_source.size(); }

	bool consume(QChar ch)
	{
		if (peek() != ch) {
			return false;
		}
		++m_pos;
		return true;
	}

	void fail(const QString &message)
	{
		if (m_error.isEmpty()) {
			m_error = QStringLiteral("%1 at character %2").arg(message).arg(m_pos);
		}
	}

	bool expect(QChar ch)
	{
		if (consume(ch)) {
			return true;
		}
		fail("unexpected character");
		return false;
	}

	QString readUntil(const QString &delimiters)
	{
		const qsizetype start = m_pos;
		while (!atEnd() && delimiters.indexOf(peek()) == -1) {
			++m_pos;
		}
		return m_source.mid(start, m_pos - start);
	}

	QString readUnit()
	{
		return readUntil(":,]})|[({");
	}

	QStringList parseConstraints()
	{
		QStringList result;
		if (!consume('(')) {
			return result;
		}
		const qsizetype start = m_pos;
		while (!atEnd() && peek() != ')') {
			++m_pos;
		}
		if (atEnd()) {
			fail("unterminated constraints");
			return {};
		}
		const QString contents = m_source.mid(start, m_pos - start);
		++m_pos;
		result = contents.split(',', Qt::KeepEmptyParts);
		return result;
	}

	QString readFieldName()
	{
		return readUntil(":,]}|");
	}

	std::optional<int64_t> parseExplicitIndex()
	{
		const qsizetype start = m_pos;
		while (!atEnd() && peek() != ',' && peek() != ']' && peek() != '}') {
			++m_pos;
		}
		return shv_number(m_source.mid(start, m_pos - start));
	}

	std::optional<Type> parseOneOf()
	{
		auto first = parseAtom();
		if (!first) {
			return {};
		}
		if (!consume('|')) {
			return first;
		}
		Type result;
		result.kind = Type::Kind::OneOf;
		result.alternatives.push_back(QSharedPointer<Type>::create(*first));
		do {
			auto alternative = parseAtom();
			if (!alternative) {
				fail("expected a type after '|'");
				return {};
			}
			result.alternatives.push_back(QSharedPointer<Type>::create(*alternative));
		}
		while (consume('|'));
		return result;
	}

	std::optional<Type> parseAtom()
	{
		if (atEnd()) {
			fail("expected a type");
			return {};
		}
		const ushort ch = peek().unicode();
		++m_pos;
		Type result;
		switch (ch) {
		case 'n': result.kind = Type::Kind::Null; return result;
		case 'b': result.kind = Type::Kind::Bool; return result;
		case 't': result.kind = Type::Kind::DateTime; return result;
		case 'i':
			if (consume('{')) {
				return parseContainer(Type::Kind::IMap, Type::Kind::Struct, '}');
			}
			if (consume('[')) {
				return parseEnum();
			}
			result.kind = Type::Kind::Int;
			result.constraints = parseConstraints();
			result.unit = readUnit();
			return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
		case 'u':
			if (consume('[')) {
				return parseBitfield();
			}
			result.kind = Type::Kind::UInt;
			result.constraints = parseConstraints();
			result.unit = readUnit();
			return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
		case 'd':
			result.kind = Type::Kind::Decimal;
			result.constraints = parseConstraints();
			result.unit = readUnit();
			return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
		case 'f':
			result.kind = Type::Kind::Double;
			result.unit = readUnit();
			return result;
		case 's':
			result.kind = Type::Kind::String;
			result.constraints = parseConstraints();
			return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
		case 'x':
			result.kind = Type::Kind::Blob;
			result.constraints = parseConstraints();
			return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
		case '[': return parseListOrTuple();
		case '{': return parseContainer(Type::Kind::Map, Type::Kind::KeyStruct, '}');
		case '?':
			result.kind = Type::Kind::Any;
			if (consume('(')) {
				result.name = readUntil(")");
				if (!expect(')')) return {};
			}
			return result;
		case '!': return parseStandard();
		default:
			fail("unknown type token");
			return {};
		}
	}

	std::optional<Type> parseEnum()
	{
		Type result;
		result.kind = Type::Kind::Enum;
		int64_t next_value = 0;
		while (true) {
			Type::Field field;
			field.name = readFieldName();
			if (field.name.isEmpty()) {
				fail("expected enum key");
				return {};
			}
			if (consume(':')) {
				field.hasExplicitIndex = true;
				field.index = parseExplicitIndex();
				if (!field.index) {
					fail("invalid enum value");
					return {};
				}
			}
			else {
				field.index = next_value;
			}
			next_value = *field.index + 1;
			result.fields.push_back(field);
			if (consume(']')) {
				break;
			}
			if (!expect(',')) {
				return {};
			}
		}
		return result;
	}

	std::optional<Type> parseBitfield()
	{
		Type result;
		result.kind = Type::Kind::BitField;
		int next_offset = 0;
		while (true) {
			auto field_type = parseOneOf();
			if (!field_type || !expect(':')) {
				return {};
			}
			Type::Field field;
			field.type = QSharedPointer<Type>::create(*field_type);
			if (!field.type) {
				fail("missing bitfield field type");
				return {};
			}
			field.name = readFieldName();
			if (field.name.isEmpty()) {
				fail("expected bitfield key");
				return {};
			}
			if (consume(':')) {
				field.hasExplicitIndex = true;
				const auto offset = parseExplicitIndex();
				if (!offset || *offset < 0 || *offset > std::numeric_limits<int>::max()) {
					fail("invalid bitfield offset");
					return {};
				}
				field.bitOffset = static_cast<int>(*offset);
			}
			else {
				field.bitOffset = next_offset;
			}
			field.index = field.bitOffset;
			next_offset = field.bitOffset + static_cast<int>(std::max<uint>(1, field.type->bitWidth()));
			result.fields.push_back(field);
			if (consume(']')) {
				break;
			}
			if (!expect(',')) {
				return {};
			}
		}
		return result;
	}

	std::optional<Type> parseListOrTuple()
	{
		auto first_type = parseOneOf();
		if (!first_type) return {};
		if (consume(':')) {
			Type result;
			result.kind = Type::Kind::Tuple;
			int64_t next_index = 0;
			auto add_field = [&result, &next_index](QSharedPointer<Type> type, QString name) {
				Type::Field field;
				field.name = name;
				field.index = next_index++;
				field.type = type;
				result.fields.push_back(field);
			};
			QString first_name = readFieldName();
			if (first_name.isEmpty()) {
				fail("expected tuple key");
				return {};
			}
			add_field(QSharedPointer<Type>::create(*first_type), first_name);
			while (consume(',')) {
				auto field_type = parseOneOf();
				if (!field_type || !expect(':')) return {};
				QString name = readFieldName();
				if (name.isEmpty()) {
					fail("expected tuple key");
					return {};
				}
				add_field(QSharedPointer<Type>::create(*field_type), name);
			}
			if (!expect(']')) {
				return {};
			}
			return result;
		}
		Type result;
		result.kind = Type::Kind::List;
		result.elementType = QSharedPointer<Type>::create(*first_type);
		if (!expect(']')) {
			return {};
		}
		result.constraints = parseConstraints();
		return m_error.isEmpty() ? std::optional<Type>(result) : std::nullopt;
	}

	std::optional<Type> parseContainer(Type::Kind generic_kind, Type::Kind struct_kind, char close)
	{
		auto first_type = parseOneOf();
		if (!first_type) return {};
		if (!consume(':')) {
			if (!expect(close)) return {};
			Type result;
			result.kind = generic_kind;
			result.elementType = QSharedPointer<Type>::create(*first_type);
			return result;
		}
		Type result;
		result.kind = struct_kind;
		int64_t next_index = 0;
		auto add_field = [&result, &next_index](QSharedPointer<Type> type, QString name, std::optional<int64_t> explicit_index) {
			Type::Field field;
			field.name = name;
			field.type = type;
			field.hasExplicitIndex = explicit_index.has_value();
			field.index = explicit_index ? explicit_index : std::optional<int64_t>(next_index);
			next_index = *field.index + 1;
			result.fields.push_back(field);
		};
		auto first_name = readFieldName();
		if (first_name.isEmpty()) {
			fail("expected struct key");
			return {};
		}
		std::optional<int64_t> first_index;
		if (consume(':')) {
			first_index = parseExplicitIndex();
			if (!first_index) {
				fail("invalid struct key index");
				return {};
			}
		}
		add_field(QSharedPointer<Type>::create(*first_type), first_name, first_index);
		while (consume(',')) {
			auto field_type = parseOneOf();
			if (!field_type || !expect(':')) return {};
			QString name = readFieldName();
			if (name.isEmpty()) {
				fail("expected struct key");
				return {};
			}
			std::optional<int64_t> index;
			if (consume(':')) {
				index = parseExplicitIndex();
				if (!index) {
					fail("invalid struct key index");
					return {};
				}
			}
			add_field(QSharedPointer<Type>::create(*field_type), name, index);
		}
		if (!expect(close)) {
			return {};
		}
		return result;
	}

	std::optional<Type> parseStandard()
	{
		const QString name = readUntil("[]{}():,|{}");
		if (name.isEmpty()) {
			fail("expected standard type name");
			return {};
		}
		Type result;
		result.kind = Type::Kind::Standard;
		result.name = name;
		if (const auto it = standard_types().constFind(name); it != standard_types().cend()) {
			QString error;
			const QString &expanded_source = it.value();
			Parser parser(expanded_source);
			auto expanded = parser.parse(&error);
			if (!expanded) {
				fail("invalid built-in standard type definition");
				return {};
			}
			result.elementType = QSharedPointer<Type>::create(*expanded);
		}
		return result;
	}

	QString m_source;
	qsizetype m_pos = 0;
	QString m_error;
};

QString RpcTypeDescription::kindName(RpcTypeDescription::Kind kind)
{
	switch (kind) {
	case Kind::Null: return QStringLiteral("Null");
	case Kind::Bool: return QStringLiteral("Bool");
	case Kind::UInt: return QStringLiteral("UInt");
	case Kind::Int: return QStringLiteral("Int");
	case Kind::Decimal: return QStringLiteral("Decimal");
	case Kind::Double: return QStringLiteral("Double");
	case Kind::Enum: return QStringLiteral("Enum");
	case Kind::String: return QStringLiteral("String");
	case Kind::Blob: return QStringLiteral("Blob");
	case Kind::DateTime: return QStringLiteral("DateTime");
	case Kind::List: return QStringLiteral("List");
	case Kind::Tuple: return QStringLiteral("Tuple");
	case Kind::IMap: return QStringLiteral("IMap");
	case Kind::Struct: return QStringLiteral("Struct");
	case Kind::Map: return QStringLiteral("Map");
	case Kind::KeyStruct: return QStringLiteral("KeyStruct");
	case Kind::BitField: return QStringLiteral("BitField");
	case Kind::OneOf: return QStringLiteral("One of");
	case Kind::Any: return QStringLiteral("Any");
	case Kind::Standard: return QStringLiteral("Standard type");
	}
	return QStringLiteral("Unknown");
}

DecodedValue RpcTypeDescription::decodeRaw(const cp::RpcValue &value, const QString &name)
{
	DecodedValue result;
	result.name = name;
	result.typeName = QString::fromLatin1(cp::RpcValue::typeToName(value.type()));
	if (value.isValueNotAvailable()) {
		result.value = QStringLiteral("n/a");
	}
	else if (value.isList() || value.isMap() || value.isIMap()) {
		result.value = containerSummary(value);
		if (value.isList()) {
			int index = 0;
			for (const auto &child : value.asList()) {
				result.children << decodeRaw(child, QStringLiteral("[%1]").arg(index++));
			}
		}
		else if (value.isMap()) {
			for (const auto &[key, child] : value.asMap()) {
				result.children << decodeRaw(child, QString::fromStdString(key));
			}
		}
		else {
			for (const auto &[key, child] : value.asIMap()) {
				result.children << decodeRaw(child, QString::number(key));
			}
		}
	}
	else if (value.isString()) {
		result.value = QString::fromStdString(value.asString());
	}
	else if (value.isBlob()) {
		result.value = QStringLiteral("Blob(%1)").arg(value.asBlob().size());
	}
	else {
		result.value = QString::fromStdString(value.toCpon());
	}
	return result;
}

QString RpcTypeDescription::containerSummary(const cp::RpcValue &value)
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

DecodedValue RpcTypeDescription::decode(const cp::RpcValue &value, const QString &name) const
{
	if (kind == Kind::Standard && elementType) {
		auto result = elementType->decode(value, name);
		result.typeName = typeName();
		return result;
	}
	if (kind == Kind::OneOf) {
		for (const auto &alternative : alternatives) {
			if (alternative && alternative->matchesValue(value)) {
				auto result = alternative->decode(value, name);
				result.typeName = typeName();
				return result;
			}
		}
	}
	if (kind == Kind::Any || (kind == Kind::Standard && !elementType)) {
		return decodeRaw(value, name);
	}

	DecodedValue result;
	result.name = name;
	result.typeName = typeName();
	result.unit = unit;
	result.description = typeConstraintsDescription().trimmed();
	if (value.isValueNotAvailable()) {
		result.value = "n/a";
		return result;
	}
	if (!matchesValue(value)) {
		return decodeRaw(value, name);
	}

	switch (kind) {
	case Kind::Null: result.value = "null"; break;
	case Kind::Bool: result.value = value.toBool() ? "true" : "false"; break;
	case Kind::UInt: result.value = QString::number(value.toUInt64()); break;
	case Kind::Int: result.value = QString::number(value.toInt64()); break;
	case Kind::Decimal:
	case Kind::Double: {
		if (kind == Kind::Decimal && constraints.size() > 2) {
			bool ok = false;
			const int precision = constraints[2].toInt(&ok);
			if (ok && precision > 0 && precision < 20) {
				result.value = QString::number(value.toDouble(), 'f', precision);
				break;
			}
		}
		result.value = QString::fromStdString(value.toCpon());
		break;
	}
	case Kind::Enum: {
		const int64_t numeric_value = value.toInt64();
		result.value = QString::number(numeric_value);
		for (const auto &field : fields) {
			if (field.index && *field.index == numeric_value) {
				result.value = QStringLiteral("%1 (%2)").arg(field.name).arg(numeric_value);
				break;
			}
		}
		break;
	}
	case Kind::String: result.value = QString::fromStdString(value.asString()); break;
	case Kind::Blob: result.value = QStringLiteral("Blob(%1)").arg(value.asBlob().size()); break;
	case Kind::DateTime: result.value = QString::fromStdString(value.toDateTime().toIsoString()); break;
	case Kind::List: {
		const auto &list = value.asList();
		result.value = containerSummary(value);
		if (elementType) {
			for (size_t i = 0; i < list.size(); ++i) {
				result.children << elementType->decode(list.value(i), QStringLiteral("[%1]").arg(i));
			}
		}
		else {
			for (size_t i = 0; i < list.size(); ++i) {
				result.children << decodeRaw(list.value(i), QStringLiteral("[%1]").arg(i));
			}
		}
		break;
	}
	case Kind::Tuple: {
		const auto &list = value.asList();
		result.value = containerSummary(value);
		QList<bool> described;
		for (size_t i = 0; i < list.size(); ++i) {
			described.append(false);
		}
		for (qsizetype i = 0; i < fields.size(); ++i) {
			const auto &field = fields[i];
			const size_t index = field.index ? static_cast<size_t>(*field.index) : static_cast<size_t>(i);
			if (index < list.size()) {
				described[static_cast<qsizetype>(index)] = true;
				result.children << (field.type ? field.type->decode(list.value(index), field.name) : decodeRaw(list.value(index), field.name));
			}
		}
		for (size_t i = 0; i < list.size(); ++i) {
			if (!described[static_cast<qsizetype>(i)]) {
				result.children << decodeRaw(list.value(i), QStringLiteral("[%1]").arg(i));
			}
		}
		break;
	}
	case Kind::IMap:
	case Kind::Struct: {
		const auto &map = value.asIMap();
		result.value = containerSummary(value);
		std::set<int> described_keys;
		for (const auto &field : fields) {
			if (field.index && *field.index >= std::numeric_limits<int>::min() && *field.index <= std::numeric_limits<int>::max()) {
				const int key = static_cast<int>(*field.index);
				if (map.hasKey(key)) {
					described_keys.insert(key);
					result.children << (field.type ? field.type->decode(map.valref(key), field.name) : decodeRaw(map.valref(key), field.name));
				}
			}
		}
		for (const auto &[key, child] : map) {
			if (!described_keys.contains(key)) result.children << decodeRaw(child, QString::number(key));
		}
		if (kind == Type::Kind::IMap && elementType && fields.isEmpty()) {
			result.children.clear();
			for (const auto &[key, child] : map) {
				result.children << elementType->decode(child, QString::number(key));
			}
		}
		break;
	}
	case Kind::Map:
	case Kind::KeyStruct: {
		const auto &map = value.asMap();
		result.value = containerSummary(value);
		std::set<std::string> described_keys;
		for (const auto &field : fields) {
			const std::string key = field.name.toStdString();
			if (map.hasKey(key)) {
				described_keys.insert(key);
				result.children << (field.type ? field.type->decode(map.valref(key), field.name) : decodeRaw(map.valref(key), field.name));
			}
		}
		for (const auto &[key, child] : map) {
			if (!described_keys.contains(key)) {
				result.children << (kind == Type::Kind::Map && elementType ? elementType->decode(child, QString::fromStdString(key)) : decodeRaw(child, QString::fromStdString(key)));
			}
		}
		break;
	}
	case Kind::BitField: {
		const uint64_t raw_value = value.toUInt64();
		result.value = QStringLiteral("0x%1 (%2)").arg(raw_value, 0, 16).arg(raw_value);
		for (const auto &field : fields) {
			if (!field.type) {
				continue;
			}
			const uint width = std::min<uint>(64, std::max<uint>(1, field.type->bitWidth()));
			const uint offset = static_cast<uint>(std::max(0, field.bitOffset));
			if (offset >= 64) continue;
			const uint effective_width = std::min<uint>(width, 64 - offset);
			const uint64_t mask = effective_width == 64 ? std::numeric_limits<uint64_t>::max() : ((uint64_t{1} << effective_width) - 1);
			uint64_t field_value = (raw_value >> offset) & mask;
			if (field.type->kind == Type::Kind::UInt && field.type->constraints.size() > 1) {
				if (const auto minimum = shv_number(field.type->constraints.front()); minimum && *minimum >= 0) field_value += static_cast<uint64_t>(*minimum);
			}
			cp::RpcValue decoded_value = field.type->kind == Type::Kind::Bool ? cp::RpcValue(field_value != 0) : cp::RpcValue(field_value);
			result.children << field.type->decode(decoded_value, field.name);
		}
		break;
	}
	case Kind::OneOf:
	case Kind::Any:
	case Kind::Standard:
		return decodeRaw(value, name);
	}
	return result;
}


QString RpcTypeDescription::typeName() const
{
	if (kind == Type::Kind::Standard) {
		return QStringLiteral("!%1").arg(name);
	}
	if (kind == Type::Kind::OneOf) {
		QStringList names;
		for (const auto &alternative : alternatives) {
			if (alternative) {
				names << alternative->typeName();
			}
		}
		return names.isEmpty() ? kindName(kind) : names.join(" | ");
	}
	return kindName(kind);
}

bool RpcTypeDescription::matchesValue(const cp::RpcValue &value) const
{
	if (kind == Type::Kind::Standard) {
		return elementType && elementType->matchesValue(value);
	}
	switch (kind) {
	case Kind::Null: return value.isNull();
	case Kind::Bool: return value.isBool();
	case Kind::UInt:
	case Kind::Int:
	case Kind::Enum:
	case Kind::BitField: return value.isInt() || value.isUInt();
	case Kind::Decimal: return value.isDecimal() || value.isInt() || value.isUInt();
	case Kind::Double: return value.isDouble() || value.isDecimal() || value.isInt() || value.isUInt();
	case Kind::String: return value.isString();
	case Kind::Blob: return value.isBlob();
	case Kind::DateTime: return value.isDateTime();
	case Kind::List:
	case Kind::Tuple: return value.isList();
	case Kind::IMap:
	case Kind::Struct: return value.isIMap();
	case Kind::Map:
	case Kind::KeyStruct: return value.isMap();
	case Kind::OneOf:
		return std::any_of(alternatives.begin(), alternatives.end(), [&value](const auto &alternative) {
			return alternative && alternative->matchesValue(value);
		});
	case Kind::Any: return true;
	case Kind::Standard: break;
	}
	return false;
}

std::optional<RpcTypeDescription> RpcTypeDescription::parse(const QString &source, QString *error)
{
	Parser parser(source);
	return parser.parse(error);
}

QString RpcTypeDescription::format(const QString &indent) const
{
	auto constraints_text = [](const QStringList &constraints) -> QString {
		if (constraints.isEmpty()) {
			return {};
		}
		return '(' + constraints.join(',') + ')';
	};
	auto field_text = [&indent](const Type::Field &field, QChar delimiter) {
		QString next_level_indent = indent + indent;
		QString line = next_level_indent;
		if (field.type) {
			line += field.type->format(next_level_indent);
			line += delimiter + field.name;
		}
		else {
			// Enum entries are labels and numeric values, not typed fields.
			line += field.name;
		}
		if (field.hasExplicitIndex && field.index) {
			line += ':' + QString::number(*field.index);
		}
		return line;
	};
	auto fields_text = [this, &field_text, &indent](QChar open, QChar close, QChar delimiter) {
		QString result(open);
		if (fields.isEmpty()) {
			result += close;
			return result;
		}
		result += '\n';
		for (qsizetype i = 0; i < fields.count(); ++i) {
			result += field_text(fields[i], delimiter);
			if (i + 1 < fields.count()) {
				result += ',';
			}
			result += '\n';
		}
		result += indent + close;
		return result;
	};
	switch (kind) {
	case Kind::Null: return QStringLiteral("n");
	case Kind::Bool: return QStringLiteral("b");
	case Kind::Int: return QStringLiteral("i") + constraints_text(constraints) + unit;
	case Kind::UInt: return QStringLiteral("u") + constraints_text(constraints) + unit;
	case Kind::Decimal: return QStringLiteral("d") + constraints_text(constraints) + unit;
	case Kind::Double: return QStringLiteral("f") + unit;
	case Kind::String: return QStringLiteral("s") + constraints_text(constraints);
	case Kind::Blob: return QStringLiteral("x") + constraints_text(constraints);
	case Kind::DateTime: return QStringLiteral("t");
	case Kind::Enum: return QStringLiteral("i") + fields_text('[', ']', ':');
	case Kind::List: return '[' + (elementType ? elementType->format(indent) : QStringLiteral("?")) + ']' + constraints_text(constraints);
	case Kind::Tuple: return fields_text('[', ']', ':');
	case Kind::IMap: return QStringLiteral("i{") + (elementType ? elementType->format(indent + indent) : QStringLiteral("?")) + '}';
	case Kind::Struct: return QStringLiteral("i") + fields_text('{', '}', ':');
	case Kind::Map: return QLatin1Char('{') + (elementType ? elementType->format(indent + indent) : QStringLiteral("?")) + '}';
	case Kind::KeyStruct: return fields_text('{', '}', ':');
	case Kind::BitField: return QStringLiteral("u") + fields_text('[', ']', ':');
	case Kind::OneOf: {
		QStringList alternatives;
		for (const auto &alternative : this->alternatives) {
			if (alternative) {
				alternatives << alternative->format(indent);
			}
		}
		return alternatives.isEmpty() ? QStringLiteral("?") : alternatives.join('|');
	}
	case Kind::Any: return name.isEmpty() ? QStringLiteral("?") : QStringLiteral("?(%1)").arg(name);
	case Kind::Standard: return QStringLiteral("!") + name;
	}
	return {};
}

QString RpcTypeDescription::typeConstraintsDescription() const
{
	if (constraints.isEmpty()) {
		return {};
	}
	auto readable_number = [](const QString &value) {
		if (const auto number = shv_number(value)) {
			return QString::number(*number);
		}
		return value;
	};
	if (constraints.count() == 1) {
		const QString constraint = constraints.front().isEmpty() ? QStringLiteral("unspecified") : readable_number(constraints.front());
		if (kind == Kind::UInt) {
			return QStringLiteral(" (maximum %1)").arg(constraint);
		}
		if (kind == Kind::String) {
			return QStringLiteral(" (exactly %1 characters)").arg(constraint);
		}
		if (kind == Kind::Blob) {
			return QStringLiteral(" (exactly %1 bytes)").arg(constraint);
		}
		if (kind == Kind::List) {
			return QStringLiteral(" (exactly %1 items)").arg(constraint);
		}
		return QStringLiteral(" (limit %1)").arg(constraint);
	}
	QString minimum = constraints[0].isEmpty() ? QStringLiteral("unbounded") : readable_number(constraints[0]);
	QString maximum = constraints[1].isEmpty() ? QStringLiteral("unbounded") : readable_number(constraints[1]);
	QString result = QStringLiteral(" (range %1 to %2").arg(minimum, maximum);
	if (constraints.count() > 2 && !constraints[2].isEmpty()) {
		result += QStringLiteral(", precision %1").arg(readable_number(constraints[2]));
	}
	return result + ')';
}

QString RpcTypeDescription::describe(int depth, const QString &field_name) const
{
	const QString pad(depth * 2, ' ');
	QString first = pad;
	if (!field_name.isEmpty()) first += field_name + QStringLiteral(": ");
	if (kind == Kind::Standard) {
		first += QStringLiteral("standard type !%1").arg(name);
		if (elementType) {
			return first + '\n' + elementType->describe(depth + 1, QStringLiteral("expands to"));
		}
		return first + QStringLiteral(" (no built-in expansion)");
	}
	if (kind == Kind::OneOf) {
		first += QStringLiteral("one of");
		for (const auto &alternative : alternatives) {
			if (alternative) {
				first += '\n' + alternative->describe(depth + 1);
			}
		}
		return first;
	}
	first += kindName(kind);
	first += typeConstraintsDescription();
	if (!unit.isEmpty()) {
		first += QStringLiteral(" [%1]").arg(unit);
	}
	if (kind == Kind::Any && !name.isEmpty()) {
		first += QStringLiteral(" (%1)").arg(name);
	}
	if (kind == Kind::List || kind == Kind::IMap || kind == Kind::Map) {
		if (elementType) {
			first += QLatin1Char('\n') + elementType->describe(depth + 1, QStringLiteral("item type"));
		}
	}
	for (const auto &field : fields) {
		QString suffix;
		if (kind == Kind::Enum && field.index) {
			suffix = QStringLiteral(" = %1").arg(*field.index);
		}
		else if ((kind == Kind::Struct || kind == Kind::BitField) && field.index) {
			suffix = QStringLiteral(" (key %1)").arg(*field.index);
			if (kind == Kind::BitField && field.type) {
				const int width = static_cast<int>(std::max<uint>(1, field.type->bitWidth()));
				suffix = QStringLiteral(" (bits %1-%2)").arg(field.bitOffset).arg(field.bitOffset + width - 1);
			}
		}
		if (field.type) {
			first += QLatin1Char('\n') + field.type->describe(depth + 1, field.name + suffix);
		}
		else {
			first += QLatin1Char('\n') + QString((depth + 1) * 2, QLatin1Char(' ')) + field.name + suffix;
		}
	}
	return first;
}

uint RpcTypeDescription::bitWidth() const
{
	if (kind == Kind::Bool) {
		return 1;
	}
	if (kind == Kind::UInt && !constraints.isEmpty()) {
		const auto maximum = shv_number(constraints.back());
		if (!maximum || *maximum < 0) {
			return 0;
		}
		int64_t range = *maximum;
		if (constraints.size() > 1) {
			const auto minimum = shv_number(constraints.front());
			if (!minimum || *minimum < 0) {
				return 0;
			}
			range = *maximum - *minimum;
		}
		return std::max<uint>(1, static_cast<uint>(std::bit_width(static_cast<uint64_t>(range))));
	}
	if (kind == Kind::Enum) {
		int64_t maximum = 0;
		for (const auto &field : fields) {
			if (field.index && *field.index >= 0) {
				maximum = std::max(maximum, *field.index);
			}
		}
		return std::max<uint>(1, static_cast<uint>(std::bit_width(static_cast<uint64_t>(maximum))));
	}
	return 0;
}
