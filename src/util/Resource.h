//
// Created by student on 3/8/24.
//

#ifndef BIRD_RESOURCE_H
#define BIRD_RESOURCE_H

namespace bird {

/**
 * Base class for engine assets supporting binary serialization and deserialization.
 */
class Resource {
public:
    Resource();
    virtual ~Resource();

    /** Deserializes asset data from disk or memory stream. */
    virtual void read() = 0;

    /** Serializes asset data into disk or memory stream. */
    virtual void write() = 0;
};

}  // namespace bird

#endif //BIRD_RESOURCE_H
