//
// Created by student on 3/8/24.
//

#ifndef BIRD_RESOURCE_H
#define BIRD_RESOURCE_H

namespace bird {

    class Resource {
    public:
        Resource();
        virtual ~Resource();

        virtual void read() = 0;
        virtual void write() = 0;

    };

}

#endif //BIRD_RESOURCE_H
