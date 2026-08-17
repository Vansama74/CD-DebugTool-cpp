#pragma once
#include <QVector>
#include <functional>
#include "IProtocolPage.h"

struct ProtocolDescriptor {
    QString key;
    QString fullName;
    std::function<IProtocolPage*(QWidget*)> factory;
};

class ProtocolRegistry {
public:
    static ProtocolRegistry& instance();
    void registerProtocol(const ProtocolDescriptor& d);
    const QVector<ProtocolDescriptor>& all() const;
    const ProtocolDescriptor* find(const QString& key) const;
private:
    ProtocolRegistry() = default;
    QVector<ProtocolDescriptor> m_protocols;
};
