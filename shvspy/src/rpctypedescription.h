#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QSharedPointer>

#include <cstdint>
#include <optional>

//type definitions here: https://silicon-heaven.github.io/shv-doc/rpctypes.html

namespace shv::chainpack { class RpcValue; }

struct DecodedValue
{
	QString name;
	QString value;
	QString typeName;
	QString unit;
	QString description;
	QList<DecodedValue> children;
};

class RpcTypeDescription
{
public:
	enum class Kind {
		Null, Bool, UInt, Int, Decimal, Double, Enum, String, Blob, DateTime,
		List, Tuple, IMap, Struct, Map, KeyStruct, BitField, OneOf, Any, Standard
	};

	static std::optional<RpcTypeDescription> parse(const QString &source, QString *error = nullptr);
	bool matchesValue(const shv::chainpack::RpcValue &value) const;
	QString format(const QString &indent = QString()) const;
	DecodedValue decode(const shv::chainpack::RpcValue &value, const QString &name = {}) const;
	QString describe(int depth, const QString &field_name = {}) const;

private:
	struct Field
	{
		QString name;
		std::optional<int64_t> index;
		bool hasExplicitIndex = false;
		int bitOffset = 0;
		QSharedPointer<RpcTypeDescription> type;
	};

	class Parser;

	Kind kind = Kind::Any;
	QString name;
	QString unit;
	QStringList constraints;
	QSharedPointer<RpcTypeDescription> elementType;
	QList<Field> fields;
	QList<QSharedPointer<RpcTypeDescription>> alternatives;

	static QString kindName(Kind kind);
	static DecodedValue decodeRaw(const shv::chainpack::RpcValue &value, const QString &name);
	static QString containerSummary(const shv::chainpack::RpcValue &value);
	QString typeName() const;
	QString typeConstraintsDescription() const;
	uint bitWidth() const;
};
