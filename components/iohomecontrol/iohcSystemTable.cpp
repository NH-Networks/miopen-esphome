#include "iohcSystemTable.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <utility>

namespace IOHC {
    iohcSystemTable *iohcSystemTable::_iohcSystemTable = nullptr;

    iohcSystemTable::iohcSystemTable() { this->load(); }

    iohcSystemTable *iohcSystemTable::getInstance() {
        if (!_iohcSystemTable)
            _iohcSystemTable = new iohcSystemTable();
        return _iohcSystemTable;
    }

    bool iohcSystemTable::addObject(address node, address backbone, uint8_t actuator[2], uint8_t manufacturer, uint8_t flags) {
        changed = true;
        std::string s0 = bytesToHexString(node, 3);
        auto *tmp = new iohcObject (node, backbone, actuator, manufacturer, flags);
        bool inserted = _objects.insert_or_assign(s0, tmp).second;
        this->save();
        return inserted;
    }

    bool iohcSystemTable::addObject(iohcObject *obj) {
        changed = true;
        std::string s0 = bytesToHexString(reinterpret_cast<uint8_t *>(obj->getNode()), 3);
        bool inserted = _objects.insert_or_assign(s0, obj).second;
        this->save();
        return inserted;
    }

    bool iohcSystemTable::addObject(std::string node_id, std::string serialized)  {
        auto *tmp = new iohcObject (std::move(serialized));
        bool inserted = _objects.insert_or_assign(node_id, tmp).second;
        this->save();
        return inserted;
    }

    bool iohcSystemTable::empty() {
        return(_objects.empty());
    }

    uint8_t iohcSystemTable::size() {
        return(_objects.size());
    }

    void iohcSystemTable::clear() {
        for (auto &kv : _objects) {
            if (kv.second) delete kv.second;
        }
        _objects.clear();
    }

    iohcSystemTable::~iohcSystemTable() {
        clear();
        if (_iohcSystemTable == this) _iohcSystemTable = nullptr;
    }

    inline iohcSystemTable::Objects::iterator iohcSystemTable::begin() {
        return(_objects.begin());
    }

    inline iohcSystemTable::Objects::iterator iohcSystemTable::end() {
        return(_objects.end());
    }

    bool iohcSystemTable::load()  {
        this->empty();
        if (!LittleFS.exists(IOHC_SYS_TABLE)) return false;

        fs::File f = LittleFS.open(IOHC_SYS_TABLE, "r", true);
        JsonDocument doc;
        deserializeJson(doc, f);
        f.close();

        for (JsonPair kv : doc.as<JsonObject>())  {
            const char* key = kv.key().c_str();
            auto obj = kv.value().as<JsonObject>();
            for (JsonPair ov : obj)
                addObject(key, ov.value().as<std::string>());
        }
        return true;
    }

    bool iohcSystemTable::save(bool force)  {
        if (!changed && force == false)
            return false;

        fs::File f = LittleFS.open(IOHC_SYS_TABLE, "a+");
        JsonDocument doc;

        for (auto [fst, snd] : _objects) {
            auto jobj = doc[fst].to<JsonObject>();
            jobj["values"] = snd->serialize();
        }
        serializeJson(doc, f);
        f.close();
        changed = false;

        return true;
    }

    void iohcSystemTable::dump1W()  {
        for (auto entry : _objects)
            entry.second->dump1W();
    }
    void iohcSystemTable::dump2W()  {
        for (auto entry : _objects)
            entry.second->dump2W();
    }
}
