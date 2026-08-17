#include "ProtocolRegistry.h"

ProtocolRegistry& ProtocolRegistry::instance()
{
    static ProtocolRegistry inst;
    return inst;
}

void ProtocolRegistry::registerProtocol(const ProtocolDescriptor& d)
{
    m_protocols.append(d);
}

const QVector<ProtocolDescriptor>& ProtocolRegistry::all() const
{
    return m_protocols;
}

const ProtocolDescriptor* ProtocolRegistry::find(const QString& key) const
{
    for (const ProtocolDescriptor& d : m_protocols) {
        if (d.key == key)
            return &d;
    }
    return nullptr;
}
