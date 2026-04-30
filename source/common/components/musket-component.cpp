#include "musket-component.hpp"

namespace our
{

    void MusketComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;   
        fireRate = data.value("fireRate", 1.0f);
        boatIndex = data.value("boatIndex", 0);
        musketIndex = data.value("musketIndex", 0);
        damage = data.value("damage", 5.0f);
        
    }

}
