#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// The loaded books, for the book list. Pages are appended as rows are inserted rather than
// replacing the whole list, so the list keeps its place and only builds the new rows.
// Each row has one role, "book": the book's fields as a map.
class BookListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_books.size(); }
    // The last book, or an empty map if there are none
    QVariantMap last() const;
    void append(const QVariantList &books);
    void clear();

private:
    QVariantList m_books;
};
