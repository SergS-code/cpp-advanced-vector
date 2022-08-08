#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include<algorithm>
#include<iterator>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {

        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }


    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};
template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_.GetAddress()+size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_.GetAddress()+size_;
    }
    const_iterator cbegin() const noexcept{

        return data_.GetAddress();
    }
    const_iterator cend() const noexcept{
        return data_.GetAddress()+size_;
    }

    Vector(){}

    Vector(Vector&& other) noexcept:data_(RawMemory<T>(std::move(other.data_))),size_(other.size_){
        other.size_=0;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos,value);

    }
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos,std::move(value));

    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        std::size_t offset_to_pos = pos - begin() ;

        if(Capacity()==size_){
            // расширение
            iterator result = nullptr;
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data.GetAddress() + std::distance(cbegin(),pos)) T(std::forward<Args>(args)...);
            if(new_data.Capacity() == 1) {
                ++size_;
                data_.Swap(new_data);
                return result;
            }

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), std::distance(cbegin(),pos), new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), std::distance(cbegin(),pos), new_data.GetAddress());
            }

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress() + std::distance(cbegin(),pos), size_ - std::distance(cbegin(),pos), new_data.GetAddress() + std::distance(cbegin(),pos) + 1);
            } else {
                std::uninitialized_copy_n(data_.GetAddress() + std::distance(cbegin(),pos), size_ - std::distance(cbegin(),pos), new_data.GetAddress() + std::distance(cbegin(),pos) + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

            size_++;
            return &data_[offset_to_pos];

        }else{
            // без расширения
            if( size_ == 0) {
                new(data_ + 0) T(std::forward<Args>(args)...);
            }
            else {
                if (pos != end()) {
                    T ptr(std::forward<Args>(args)...);
                    std::uninitialized_move_n(end() - 1, 1, end());
                    if (pos != end() - 1)
                        std::move_backward(data_.GetAddress() + offset_to_pos, end() - 1, end());
                    *(data_ + offset_to_pos) = std::move(ptr);
                } else {
                    new (data_ + offset_to_pos) T(std::forward<Args>(args)...);
                }

            }
            size_++;
            return &data_[offset_to_pos];
        }


    }
    iterator Erase(const_iterator pos){
        std::size_t offset_to_pos = pos - begin() ;
        if(offset_to_pos==size_-1){
            return end();
        }
        std::move(data_.GetAddress()+offset_to_pos+1,end(),data_.GetAddress()+offset_to_pos);
        PopBack();
        return &data_[offset_to_pos];

    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){

        if (size_ == Capacity()) {
            RawMemory<T> rnew_data(size_ == 0 ? 1 : size_ * 2);
            new(rnew_data + size_)T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, rnew_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, rnew_data.GetAddress());
            }
            data_.Swap(rnew_data);
            std::destroy_n(rnew_data.GetAddress(), rnew_data.Capacity());
            ++size_;
        }
        else {
            new(data_.GetAddress()+size_)T(std::forward<Args>(args)...);
            ++size_;
        }
        return data_[size_-1];
    }



    Vector& operator=(const Vector& rhs){
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector temp(rhs);
                Swap(temp);
            } else {
                if(rhs.Size()<this->Size()){
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy(data_.GetAddress()+rhs.size_,data_.GetAddress()+size_);
                    size_ = rhs.size_;
                }else{
                    std::copy_n(rhs.data_.GetAddress(),size_, data_.GetAddress());
                    std::uninitialized_copy(rhs.data_.GetAddress()+size_, rhs.data_.GetAddress() + rhs.size_, data_.GetAddress()+size_);
                    size_ = rhs.size_;
                }
            }
        }
        return *this;

    }
    Vector& operator=(Vector&& rhs) noexcept{

        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;


    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        //  std::swap(data_,other.data_);
        std::swap(this->size_,other.size_);
    }

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);

    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_,data_.GetAddress());

    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }


    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }


    void Reserve(size_t new_capacity) {

        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);

    }

    void Resize(size_t new_size){
        Reserve(new_size);
        if(new_size>size_){
            std::uninitialized_value_construct_n(data_.GetAddress()+size_,new_size-size_);
        }else{
            std::destroy_n(data_.GetAddress()+new_size, size_ - new_size);
        }
        size_=new_size;

    }

    void PopBack()  noexcept {
        std::destroy_n(data_.GetAddress()+size_-1, 1);
        size_--;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
