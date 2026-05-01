#include "musket-component.hpp"

namespace our
{

    void MusketComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        modelName = data.value("modelName", modelName);
        fireRate = data.value("fireRate", fireRate);
        boatIndex = data.value("boatIndex", boatIndex);
        musketIndex = data.value("musketIndex", musketIndex);
        damage = data.value("damage", damage);
    }

}
