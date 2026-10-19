//
//  UnityYaml.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnityYaml.h"

#include <QRegularExpression>
#include <QStringList>

namespace unity {

namespace {

// A non-blank line: its indentation and its text without the indentation or trailing spaces.
struct Line {
    int indent { 0 };
    QString text;
    int number { 0 };       // in the file, from 1
    int blankBefore { 0 };  // blank lines just above it: inside a multi-line string each one is a line break
};

void addLine(QVector<Line>& lines, const QString& raw, int number, int& blanks) {
    int indent = 0;
    while (indent < raw.size() && raw[indent] == ' ') {
        indent++;
    }
    int end = raw.size();
    while (end > indent && (raw[end - 1] == ' ' || raw[end - 1] == '\t' || raw[end - 1] == '\r')) {
        end--;
    }
    if (end > indent) {
        lines.push_back({ indent, raw.mid(indent, end - indent), number, blanks });
        blanks = 0;
    } else {
        blanks++;
    }
}

// How a continuation line joins a multi-line string: a single line break folds into a space, and each blank
// line in between stands for one line break.
QString joinFolded(const QString& text, const Line& line) {
    return text + (line.blankBefore > 0 ? QString(line.blankBefore, '\n') : QStringLiteral(" ")) + line.text;
}

bool isItem(const QString& text) {
    return text == QLatin1String("-") || text.startsWith(QLatin1String("- "));
}

// The colon of "key: value" or "key:", or -1 when the text is not a mapping entry.
int keyColon(const QString& text) {
    if (text.isEmpty() || isItem(text)) {
        return -1;
    }
    const QChar first = text[0];
    if (first == '"' || first == '\'') {
        int end = 1;
        while (end < text.size()) {
            if (first == '"' && text[end] == '\\') {
                end += 2;
                continue;
            }
            if (text[end] == first) {
                if (first == '\'' && end + 1 < text.size() && text[end + 1] == '\'') {
                    end += 2;
                    continue;
                }
                break;
            }
            end++;
        }
        if (end + 1 < text.size() && text[end + 1] == ':' && (end + 2 == text.size() || text[end + 2] == ' ')) {
            return end + 1;
        }
        return -1;
    }
    if (first == '{' || first == '[') {
        return -1;
    }
    for (int k = 0; k < text.size(); k++) {
        const QChar c = text[k];
        if (c == ':' && (k + 1 == text.size() || text[k + 1] == ' ')) {
            return k;
        }
        if (c == '{' || c == '[') {
            return -1;
        }
    }
    return -1;
}

QString unquote(const QString& s) {
    if (s.size() >= 2 && s.startsWith('\'') && s.endsWith('\'')) {
        return s.mid(1, s.size() - 2).replace(QLatin1String("''"), QLatin1String("'"));
    }
    if (s.size() >= 2 && s.startsWith('"') && s.endsWith('"')) {
        QString out;
        out.reserve(s.size());
        for (int k = 1; k < s.size() - 1; k++) {
            const QChar c = s[k];
            if (c != '\\' || k + 1 >= s.size() - 1) {
                out += c;
                continue;
            }
            const QChar e = s[++k];
            switch (e.unicode()) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '0': out += QChar(0); break;
                case ' ': out += ' '; break;
                case 'x':
                case 'u':
                case 'U': {
                    const int digits = e == 'x' ? 2 : e == 'u' ? 4 : 8;
                    bool ok = false;
                    const uint code = s.mid(k + 1, digits).toUInt(&ok, 16);
                    if (ok) {
                        if (code > 0xffff) {
                            out += QString::fromUcs4(&code, 1);
                        } else {
                            out += QChar(code);
                        }
                        k += digits;
                    }
                    break;
                }
                default: out += e; break;  // \" \\ \/ and anything else stand for themselves
            }
        }
        return out;
    }
    return s;
}

// Whether every { and [ outside quotes is closed.
bool balanced(const QString& s) {
    int depth = 0;
    QChar quote;
    for (int k = 0; k < s.size(); k++) {
        const QChar c = s[k];
        if (!quote.isNull()) {
            if (quote == '"' && c == '\\') {
                k++;
            } else if (c == quote) {
                quote = QChar();
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            depth--;
        }
    }
    return depth <= 0;
}

// Whether a quoted scalar that starts at s[0] is closed by the end of s.
bool closedQuote(const QString& s) {
    const QChar q = s[0];
    for (int k = 1; k < s.size(); k++) {
        if (q == '"' && s[k] == '\\') {
            k++;
            continue;
        }
        if (s[k] == q) {
            if (q == '\'' && k + 1 < s.size() && s[k + 1] == '\'') {
                k++;
                continue;
            }
            return true;
        }
    }
    return false;
}

class Parser {
public:
    explicit Parser(const QVector<Line>& lines) : _lines(lines) {}

    QVariant parseAll() {
        int i = 0;
        if (_lines.isEmpty()) {
            return QVariant();
        }
        QVariant value = parseNode(i);
        if (i < size()) {
            fail(i, "text after the end of the document");
        }
        return value;
    }

    const QString& error() const { return _error; }

private:
    const QVector<Line>& _lines;
    QString _error;

    int size() const { return _lines.size(); }

    void fail(int i, const QString& what) {
        if (_error.isEmpty()) {
            _error = QString("line %1: %2").arg(i < size() ? _lines[i].number : -1).arg(what);
        }
    }

    QVariant parseNode(int& i) {
        const Line& line = _lines[i];
        if (isItem(line.text)) {
            return parseSequence(i, line.indent);
        }
        if (keyColon(line.text) >= 0) {
            return parseMapping(i, line.indent, nullptr);
        }
        const QString text = line.text;
        i++;
        return parseValue(text, i, line.indent - 1);
    }

    // A block mapping whose keys sit at this indentation. With firstText, the first entry is the rest of a
    // "- key: value" list item, which counts as sitting at the indentation of the entries that follow it.
    QVariantMap parseMapping(int& i, int indent, const QString* firstText) {
        QVariantMap map;
        bool first = firstText != nullptr;
        while (i < size()) {
            const Line& line = _lines[i];
            if (!first) {
                if (line.indent < indent || (line.indent == indent && isItem(line.text))) {
                    break;
                }
                if (line.indent > indent) {
                    fail(i, "unexpected indentation");
                    i++;
                    continue;
                }
            }
            const QString text = first ? *firstText : line.text;
            first = false;
            const int colon = keyColon(text);
            if (colon < 0) {
                fail(i, "expected a key");
                i++;
                continue;
            }
            const QString key = unquote(text.left(colon).trimmed());
            const QString rest = text.mid(colon + 1).trimmed();
            i++;
            if (!rest.isEmpty()) {
                map.insert(key, parseValue(rest, i, indent));
            } else if (i < size() && _lines[i].indent > indent) {
                map.insert(key, parseNode(i));
            } else if (i < size() && _lines[i].indent == indent && isItem(_lines[i].text)) {
                map.insert(key, parseSequence(i, indent));  // Unity writes a key's list at the key's own indentation
            } else {
                map.insert(key, QVariant());
            }
        }
        return map;
    }

    QVariantList parseSequence(int& i, int indent) {
        QVariantList list;
        while (i < size()) {
            const Line& line = _lines[i];
            if (line.indent < indent || (line.indent == indent && !isItem(line.text))) {
                break;
            }
            if (line.indent > indent) {
                fail(i, "unexpected indentation in a list");
                i++;
                continue;
            }
            QString rest = line.text.size() > 1 ? line.text.mid(2) : QString();
            int extra = 0;
            while (extra < rest.size() && rest[extra] == ' ') {
                extra++;
            }
            rest = rest.mid(extra);
            if (rest.isEmpty()) {
                i++;
                list.append(i < size() && _lines[i].indent > indent ? parseNode(i) : QVariant());
            } else if (keyColon(rest) >= 0) {
                list.append(parseMapping(i, indent + 2 + extra, &rest));
            } else if (isItem(rest)) {
                fail(i, "a list inside a list on one line");
                i++;
            } else {
                i++;
                list.append(parseValue(rest, i, indent));
            }
        }
        return list;
    }

    // A value written after "key:" or "- ". Lines indented deeper than parentIndent continue it.
    QVariant parseValue(QString text, int& i, int parentIndent) {
        const QChar c = text[0];
        if (c == '{' || c == '[') {
            while (!balanced(text) && i < size() && _lines[i].indent > parentIndent) {
                text += ' ' + _lines[i++].text;
            }
            int p = 0;
            return parseFlow(text, p);
        }
        if (c == '"' || c == '\'') {
            while (!closedQuote(text) && i < size() && _lines[i].indent > parentIndent) {
                text = joinFolded(text, _lines[i++]);
            }
            return unquote(text);
        }
        if ((c == '|' || c == '>') && text.size() <= 3) {
            QStringList parts;
            while (i < size() && _lines[i].indent > parentIndent) {
                parts << _lines[i++].text;
            }
            return parts.join(c == '|' ? QStringLiteral("\n") : QStringLiteral(" "));
        }
        while (i < size() && _lines[i].indent > parentIndent) {
            text = joinFolded(text, _lines[i++]);
        }
        return text;
    }

    static void skip(const QString& s, int& p) {
        while (p < s.size() && s[p] == ' ') {
            p++;
        }
    }

    // A scalar inside a flow collection: quoted, or plain up to , } ] (and up to ": " for a key).
    static QString flowScalar(const QString& s, int& p, bool isKey) {
        skip(s, p);
        const int start = p;
        if (p < s.size() && (s[p] == '"' || s[p] == '\'')) {
            const QChar q = s[p++];
            while (p < s.size()) {
                if (q == '"' && s[p] == '\\') {
                    p += 2;
                    continue;
                }
                if (s[p] == q) {
                    if (q == '\'' && p + 1 < s.size() && s[p + 1] == '\'') {
                        p += 2;
                        continue;
                    }
                    p++;
                    break;
                }
                p++;
            }
            return s.mid(start, p - start);
        }
        while (p < s.size()) {
            const QChar c = s[p];
            if (c == ',' || c == '}' || c == ']') {
                break;
            }
            if (isKey && c == ':' && (p + 1 >= s.size() || s[p + 1] == ' ' || s[p + 1] == ',' || s[p + 1] == '}')) {
                break;
            }
            p++;
        }
        return s.mid(start, p - start).trimmed();
    }

    QVariant flowValue(const QString& s, int& p) {
        skip(s, p);
        if (p < s.size() && (s[p] == '{' || s[p] == '[')) {
            return parseFlow(s, p);
        }
        const QString v = flowScalar(s, p, false);
        return v.isEmpty() ? QVariant() : QVariant(unquote(v));
    }

    QVariant parseFlow(const QString& s, int& p) {
        skip(s, p);
        if (p >= s.size()) {
            return QVariant();
        }
        if (s[p] == '{') {
            p++;
            QVariantMap map;
            while (true) {
                skip(s, p);
                if (p >= s.size()) {
                    _error = _error.isEmpty() ? QString("unclosed { in: %1").arg(s.left(80)) : _error;
                    break;
                }
                if (s[p] == '}') {
                    p++;
                    break;
                }
                const QString key = unquote(flowScalar(s, p, true));
                skip(s, p);
                QVariant value;
                if (p < s.size() && s[p] == ':') {
                    p++;
                    value = flowValue(s, p);
                }
                map.insert(key, value);
                skip(s, p);
                if (p < s.size() && s[p] == ',') {
                    p++;
                } else if (p < s.size() && s[p] != '}') {
                    p++;  // tolerate junk rather than loop
                }
            }
            return map;
        }
        if (s[p] == '[') {
            p++;
            QVariantList list;
            while (true) {
                skip(s, p);
                if (p >= s.size()) {
                    _error = _error.isEmpty() ? QString("unclosed [ in: %1").arg(s.left(80)) : _error;
                    break;
                }
                if (s[p] == ']') {
                    p++;
                    break;
                }
                list.append(flowValue(s, p));
                skip(s, p);
                if (p < s.size() && s[p] == ',') {
                    p++;
                } else if (p < s.size() && s[p] != ']') {
                    p++;
                }
            }
            return list;
        }
        return flowValue(s, p);
    }
};

void setError(QString* error, const QString& message) {
    if (error && error->isEmpty() && !message.isEmpty()) {
        *error = message;
    }
}

}  // namespace

QVector<YamlObject> parseObjects(const QByteArray& bytes, QString* error) {
    static const QRegularExpression HEADER(QStringLiteral("^--- !u!(-?\\d+) &(-?\\d+)( stripped)?\\s*$"));
    const QStringList raw = QString::fromUtf8(bytes).split('\n');
    QVector<YamlObject> objects;
    QVector<Line> body;
    YamlObject current;
    bool inObject = false;
    int blanks = 0;

    auto finish = [&]() {
        if (!inObject) {
            return;
        }
        Parser parser(body);
        const QVariantMap top = parser.parseAll().toMap();
        if (!parser.error().isEmpty()) {
            setError(error, QString("object %1: %2").arg(current.fileID).arg(parser.error()));
        }
        if (!top.isEmpty()) {
            current.type = top.firstKey();
            current.fields = top.first().toMap();
        }
        objects.push_back(current);
        body.clear();
    };

    for (int n = 0; n < raw.size(); n++) {
        const QString& line = raw[n];
        if (line.startsWith(QLatin1String("---"))) {
            finish();
            const auto match = HEADER.match(line.trimmed());
            current = YamlObject();
            blanks = 0;
            inObject = match.hasMatch();
            if (inObject) {
                current.classID = match.captured(1).toInt();
                current.fileID = match.captured(2).toLongLong();
                current.stripped = !match.captured(3).isEmpty();
            }
            continue;
        }
        if (inObject) {
            addLine(body, line, n + 1, blanks);
        }
    }
    finish();
    return objects;
}

QVariant parseDocument(const QByteArray& bytes, QString* error) {
    const QStringList raw = QString::fromUtf8(bytes).split('\n');
    QVector<Line> lines;
    int blanks = 0;
    for (int n = 0; n < raw.size(); n++) {
        if (raw[n].startsWith('%') || raw[n].startsWith(QLatin1String("---"))) {
            continue;
        }
        addLine(lines, raw[n], n + 1, blanks);
    }
    Parser parser(lines);
    QVariant value = parser.parseAll();
    setError(error, parser.error());
    return value;
}

qint64 fileIDOf(const QVariant& reference) {
    return reference.toMap().value(QStringLiteral("fileID")).toString().toLongLong();
}

QString guidOf(const QVariant& reference) {
    return reference.toMap().value(QStringLiteral("guid")).toString();
}

double toNumber(const QVariant& value, double fallback) {
    bool ok = false;
    const double number = value.toString().toDouble(&ok);
    return ok ? number : fallback;
}

QVariantMap mergeKeyList(const QVariant& list) {
    QVariantMap merged;
    for (const QVariant& entry : list.toList()) {
        const QVariantMap map = entry.toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            merged.insert(it.key(), it.value());
        }
    }
    return merged;
}

}  // namespace unity
