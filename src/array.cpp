#include <stdlib.h>
#include <initializer_list>

template <class T>
class Array {
public:
    T *data = nullptr;
    size_t count = 0;
    size_t capacity = 0;

    void reserve(size_t num_elements);

    Array() {
        data = nullptr;
        count = 0;
        capacity = 0;
    }

    Array(std::initializer_list<T> list) {
        reserve(list.size());
        for (auto it = list.begin(); it != list.end(); it++) {
            push(*it);
        }
    }
    
    void grow(size_t num_elements) {
        size_t new_cap = 0;
        while (new_cap < capacity + num_elements) {
            new_cap = 2 * new_cap + 1;
        }
        data = (T *)realloc(data, new_cap * sizeof(T));
        capacity = new_cap;
    }
    
    void push(T element) {
        if (count + 1 > capacity) {
            grow(1);
        }
        data[count] = element;
        count++;
    }

    T operator[](size_t index) {
        assert(index < count);
        return data[index];
    }

    bool empty() { return count == 0; }

    void clear() {
        if (data) {
            free(data);
        }
        data = nullptr;
        capacity = count = 0;
    }

    T* begin(){
        return data;
    }
    T* end() {
        return data + count;
    }

//     Array(Array<T> &array) {
//         Array<T> copy{};
//         copy.reserve(array.count);
//         copy.count = array.count;
//         memcpy(copy.data, array.data, array.count * sizeof(T));
//     }

};

template<typename T>
void Array<T>::reserve(size_t num_elements) {
    data = (T *)malloc(num_elements * sizeof(T));
    capacity = num_elements;
}


