#ifndef BLOCKINFO_H
#define BLOCKINFO_H

#include <QObject>
#include <QUrl>
#include <qqml.h>
#include <QQmlEngine>
#include <QJsonObject>

class BlockInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString textHtml READ textHtml WRITE setTextHtml NOTIFY textHtmlChanged)
    Q_PROPERTY(QString textPlainText READ textPlainText WRITE setTextPlainText NOTIFY textPlainTextChanged)
    Q_PROPERTY(BlockType blockType READ blockType WRITE setBlockType NOTIFY blockTypeChanged)
    Q_PROPERTY(unsigned int lineStartPos READ lineStartPos WRITE setLineStartPos NOTIFY lineStartPosChanged)
    Q_PROPERTY(unsigned int lineEndPos READ lineEndPos WRITE setLineEndPos NOTIFY lineEndPosChanged)
    Q_PROPERTY(unsigned int totalIndentLength READ totalIndentLength WRITE setTotalIndentLength NOTIFY totalIndentLengthChanged)
    Q_PROPERTY(QList<BlockInfo *> children READ children WRITE setChildren NOTIFY childrenChanged)
    Q_PROPERTY(BlockInfo *parent READ parent WRITE setParent NOTIFY parentChanged)
    Q_PROPERTY(unsigned int indentLevel READ indentLevel WRITE setIndentLevel NOTIFY indentLevelChanged)
    Q_PROPERTY(QString blockDelimiter READ blockDelimiter WRITE setBlockDelimiter NOTIFY blockDelimiterChanged)
    Q_PROPERTY(QString indentedString READ indentedString WRITE setIndentedString NOTIFY indentedStringChanged)
    Q_PROPERTY(QJsonObject metaData READ metaData WRITE setMetaData NOTIFY metaDataChanged)

public:
    enum class BlockType {
        RegularText,
        Quote,
        BulletListItem,
        NumberedListItem,
        Heading,
        Todo,
        Divider,
        DropCap
    };
    Q_ENUM(BlockType)

    explicit BlockInfo(QObject *parent = nullptr);

    QString textHtml() const;
    void setTextHtml(const QString &newTextHtml);

    QString textPlainText() const;
    void setTextPlainText(const QString &newTextPlainText);

    BlockType blockType() const;
    void setBlockType(const BlockType &newType);

    BlockType determineBlockType(QString text);

    unsigned int lineStartPos() const;
    void setLineStartPos(unsigned int newLineStartPos);

    unsigned int lineEndPos() const;
    void setLineEndPos(unsigned int newLineEndPos);

    unsigned int totalIndentLength() const;
    void setTotalIndentLength(unsigned int newTotalIndentLength);

    QList<BlockInfo *> children() const;
    void setChildren(const QList<BlockInfo *> &newChildren);
    void addChild(BlockInfo *newChild);
    void removeChild(BlockInfo* child);

    BlockInfo *parent() const;
    void setParent(BlockInfo *newParent);

    unsigned int indentLevel() const;
    void setIndentLevel(unsigned int newIndentLevel);

    bool isBlockListItem();

    QString blockDelimiter() const;
    void setBlockDelimiter(const QString &newBlockDelimiter);

    bool isNumberedList(QString str);

    QString indentedString() const;
    void setIndentedString(const QString &newIndentedString);

    bool isIndentable(); // TODO: do we need this function now since everything is indentable?

    QJsonObject metaData() const;
    void setMetaData(const QJsonObject &newMetaData);
    void updateMetaData(const QString key, const QVariant value);

signals:
    void textHtmlChanged();
    void textPlainTextChanged();
    void blockTypeChanged();

    void lineStartPosChanged();

    void lineEndPosChanged();

    void totalIndentLengthChanged();

    void childrenChanged();

    void parentChanged();

    void indentLevelChanged();

    void blockDelimiterChanged();

    void indentedStringChanged();

    void metaDataChanged();

private:
    QString m_textHtml;
    QString m_textPlainText;
    BlockType m_blockType;
    unsigned int m_lineStartPos;
    unsigned int m_lineEndPos;
    unsigned int m_totalIndentLength;
    unsigned int m_indentLevel;
    QList<BlockInfo*> m_children;
    BlockInfo* m_parent;
    QString m_blockDelimiter;
    QString m_indentedString;
    QJsonObject m_metaData;

    QString trimLeadingWhitespaces(const QString &str);
};

#endif // BLOCKINFO_H
