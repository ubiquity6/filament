//
// Created by Aleksander on 2/20/19.
//

#ifndef HELLOREACT_JSTYPEDARRAY_H
#define HELLOREACT_JSTYPEDARRAY_H


struct TypedArray {
    size_t byteLength;
    uint8_t * data;

    TypedArray(uint8_t * data, size_t byteLength)
    {
        this->byteLength = byteLength;
        this->data = data;
    }
};

#endif //HELLOREACT_JSTYPEDARRAY_H
