#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t u64;
typedef uint8_t  u8;

typedef int64_t isize;
//#define BIG_INT_TESTING

#ifdef BIG_INT_TESTING
    typedef uint8_t Limb;
#else
    typedef uint64_t Limb;
#endif

typedef struct Big_Int {
    Limb* data;
    int32_t size;
    int32_t capacity;
    int64_t sign;
} Big_Int;

#define ASSERT(x) (!(x) ? abort() : 0)

Limb _big_int_mul_doubleword(Limb* high, Limb a, Limb b);
u8 _big_int_add_carry1(Limb* out, Limb a, Limb b, u8 carry);
u8 _big_int_sub_borrow1(Limb* out, Limb a, Limb b, u8 carry);
u8 _big_int_add_carry2(Limb* out, Limb a, Limb b, u8 carry);
u8 _big_int_sub_borrow2(Limb* out, Limb a, Limb b, u8 carry);


void _big_int_check_invariants(Big_Int a)
{
    ASSERT((a.capacity != 0) == (a.data != NULL));
    ASSERT(a.size <= a.capacity);
    ASSERT(a.size == 0 || a.data[a.size - 1] != 0);
    ASSERT(a.sign == 1 || a.sign == -1 || a.sign == 0);
}

void big_int_reserve(Big_Int* out, isize capacity)
{
    _big_int_check_invariants(*out);
    if(out->capacity < capacity)
    {
        isize new_capacity = out->capacity*3/2 + 8;
        if(new_capacity < capacity)
            new_capacity = capacity;

        Limb* new_data = (Limb*) realloc(out->data, new_capacity*sizeof(Limb));
        ASSERT(new_data != NULL);

        out->data = (int32_t) new_data;
        out->capacity = (int32_t) new_capacity;
    }
    _big_int_check_invariants(*out);
}

void big_int_deinit(Big_Int* out)
{
    if(out)
    {
        _big_int_check_invariants(*out);
        free(out->data);
        memset(out, 0, sizeof *out);
        _big_int_check_invariants(*out);
    }
}

isize _big_int_find_last_nonzero(const Limb* limbs, isize count)
{
    for(isize i = count; i-- > 0; )
        if(limbs[i] > 0)
            return i + 1;

    return 0;
}

void big_int_assign_limbs(Big_Int* out, const Limb* limbs, isize count, isize sign)
{
    _big_int_check_invariants(*out);
    isize nonzero_count = _big_int_find_last_nonzero(limbs, count);
    ASSERT(sign == 1 || sign == -1);

    big_int_reserve(out, nonzero_count);
    memcpy(out->data, limbs, nonzero_count * sizeof *limbs);
    out->sign = sign;
    out->size = (int32_t) nonzero_count;
}

void big_int_assign_unsigned(Big_Int* out, Limb value, isize sign)
{
    if(value == 0)
        out->size = 0;
    else
        big_int_assign_limbs(out, &value, 1, sign);
}

isize _big_int_assign(Limb* limbs, uint64_t value)
{
    if(sizeof(Limb) == sizeof(value))
    {
        limbs[0] = value;
        return value ? 1 : 0;
    }
    else
    {
        uint64_t LIMB_DIV = 1ull << (sizeof(Limb) == 8 ? 0 : sizeof(Limb)*8);
        isize limbs_count = 0;
        while(value > 0)
        {
            limbs[limbs_count ++] = value % LIMB_DIV;
            value /= LIMB_DIV;
        }
        return limbs_count;
    }
}

void big_int_assign(Big_Int* out, isize value)
{
    if(value == 0)
        out->size = 0;
    else
    {
        uint64_t abs_val = value > 0 ? (uint64_t) value : (uint64_t) -value;
        isize sign =    value > 0 ? 1 : -1;
        
        if(sizeof(Limb) == sizeof(abs_val))
        {
            big_int_assign_limbs(out, &abs_val, 1, sign);
        }
        else
        {
            uint64_t LIMB_DIV = 1ull << (sizeof(Limb) == 8 ? 0 : sizeof(Limb)*8);

            Limb limbs[8] = {0};
            isize limbs_count = 0;
            while(abs_val > 0)
            {
                limbs[limbs_count ++] = abs_val % LIMB_DIV;
                abs_val /= LIMB_DIV;
            }
            big_int_assign_limbs(out, limbs, limbs_count, sign);
        }
    }
}

void big_int_assign_sized_string(Big_Int* out, const char* str, isize size)
{
    _big_int_check_invariants(*out);
    ASSERT(false && "TODO");
    (void) out;
    (void) str;
    (void) size;
}

void big_int_assign_string(Big_Int* out, const char* str)
{
    isize size = str ? (isize) strlen(str) : 0;
    big_int_assign_sized_string(out, str, size);
}


Big_Int big_int_make(isize value)
{
    Big_Int out = {0};
    big_int_assign(&out, value);
    return out;
}

Big_Int big_int_make_from_str(const char* str)
{
    Big_Int out = {0};
    big_int_assign_string(&out, str);
    return out;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

//|out| = |a| + |b|
void big_int_add_abs(Big_Int* out, Big_Int a, Big_Int b)
{
    _big_int_check_invariants(*out);
    _big_int_check_invariants(a);
    _big_int_check_invariants(b);

    Big_Int smaller = a.size < b.size ? a : b;
    Big_Int bigger = a.size > b.size ? a : b;
    big_int_reserve(out, bigger.size + 1);

    Limb carry = 0;
    isize i = 0;
    for(; i < smaller.size; i++)
    {
        Limb sum = a.data[i] + b.data[i] + carry;
        carry = sum < a.data[i] || sum < b.data[i];

        out->data[i] = sum;
    }

    if(carry)
    {
        for(; i < bigger.size; i++)
        {
            Limb before = bigger.data[i];
            Limb sum = before + carry;

            out->data[i] = sum;
            if(sum >= before)
                goto copy;
        }

        out->data[i++] = carry;
        out->size = (int32_t) i;
        _big_int_check_invariants(*out);
        return;
    }

    copy:
    memcpy(out->data + i, bigger.data + i, (bigger.size - i)*sizeof(Limb));
    out->size = (int32_t) i;
    
    _big_int_check_invariants(*out);
}

//|out| = |a| - |b| 
//If |a| >= |b| returns true.
//If |a| < |b| returns false and |out| will contain the conjugated value.
bool big_int_sub_abs(Big_Int* out, Big_Int a, Big_Int b)
{
    _big_int_check_invariants(*out);
    _big_int_check_invariants(a);
    _big_int_check_invariants(b);

    big_int_reserve(out, a.size);

    Limb carry = 0;
    isize i = 0;
    for(; i < b.size; i++)
    {
        Limb diff = a.data[i] - (b.data[i] + carry);
        carry = diff > a.data[i] || (b.data[i] + carry < b.data[i]);

        out->data[i] = diff;
    }

    if(carry)
    {
        for(; i < a.size; i++)
        {
            Limb before = a.data[i];
            Limb diff = before - carry;

            out->data[i] = diff;
            if(diff <= before)
                goto copy;
        }

        out->size = (int32_t) i;
        _big_int_check_invariants(*out);
        return false;
    }

    copy:
    memcpy(out->data + i, a.data + i, (a.size - i)*sizeof(Limb));
    out->size = (int32_t) i;

    _big_int_check_invariants(*out);
    return true;
}

int big_int_compare_abs(Big_Int a, Big_Int b)
{
    _big_int_check_invariants(a);
    _big_int_check_invariants(b);

    if(a.size != b.size)
        return a.size < b.size ? -1 : 1;
    
    for(isize i = a.size; i-- > 0;)
        if(a.data[i] != b.data[i])
            return a.data[i] < b.data[i] ? -1 : 1;

    return 0;
}

int big_int_compare(Big_Int a, Big_Int b)
{
    _big_int_check_invariants(a);
    _big_int_check_invariants(b);

    if(a.size == 0 && b.size == 0)
        return 0;

    if(a.sign != b.sign)
    {
        if(a.sign < 0)
            return -1;
        else
            return 1;
    }

    int sign = (int) (int32_t)a.sign;
    return big_int_compare_abs(a, b)*sign;
}

void big_int_add(Big_Int* out, Big_Int a, Big_Int b)
{
    _big_int_check_invariants(*out);
    _big_int_check_invariants(a);
    _big_int_check_invariants(b);

    //add
    if(a.sign == b.sign)
    {
        big_int_add_abs(out, a, b);
        out->sign = a.sign;
    }
    //sub
    else
    {
        int sign = a.sign;
        int comp = big_int_compare_abs(a, b);
        if(comp == 0)
        {
            out->sign = 1;
            out->size = 0;
        }
        else
        {
            Big_Int bigger = comp > 0 ? a : b;
            Big_Int smaller = comp > 0 ? b : a;

            big_int_sub_abs(out, bigger, smaller);
            out->sign = sign*comp;
        }
    }

    _big_int_check_invariants(*out);
}

void big_int_mul_karatsuba(Big_Int* out, Big_Int a, Big_Int b);

u8 _big_int_add_carry(Limb* dest1, const Limb* a1, const Limb* b1, isize size, u8 carry)
{
    u8 carry1 = carry;
    isize i = 0;
    for(; i <= size - 4; i += 4)
    {
        carry1 = _big_int_add_carry1(dest1 + i+0, a1[i+0], b1[i+0], carry1);
        carry1 = _big_int_add_carry1(dest1 + i+1, a1[i+1], b1[i+1], carry1);
        carry1 = _big_int_add_carry1(dest1 + i+2, a1[i+2], b1[i+2], carry1);
        carry1 = _big_int_add_carry1(dest1 + i+3, a1[i+3], b1[i+3], carry1);
    }

    for(; i < size; i ++)
    {
        carry1 = _big_int_add_carry1(dest1 + i+0, a1[i+0], b1[i+0], carry1);
    }

    return carry1;
}

typedef struct {
    u8 carry1;
    u8 carry2;
} Carries;

Carries _big_int_add_conc_carry(
    isize size, Carries carry,
    Limb* dest1, const Limb* a1, const Limb* b1, 
    Limb* dest2, const Limb* a2, const Limb* b2)
{
    Carries c = carry;
    isize i = 0;
    for(; i <= size - 4; i += 4)
    {
        c.carry1 = _big_int_add_carry1(dest1 + i+0, a1[i+0], b1[i+0], c.carry1);
        c.carry2 = _big_int_add_carry2(dest2 + i+0, a2[i+0], b2[i+0], c.carry2);
        
        c.carry1 = _big_int_add_carry1(dest1 + i+1, a1[i+1], b1[i+1], c.carry1);
        c.carry2 = _big_int_add_carry2(dest2 + i+1, a2[i+1], b2[i+1], c.carry2);
        
        c.carry1 = _big_int_add_carry1(dest1 + i+2, a1[i+2], b1[i+2], c.carry1);
        c.carry2 = _big_int_add_carry2(dest2 + i+2, a2[i+2], b2[i+2], c.carry2);
        
        c.carry1 = _big_int_add_carry1(dest1 + i+3, a1[i+3], b1[i+3], c.carry1);
        c.carry2 = _big_int_add_carry2(dest2 + i+3, a2[i+3], b2[i+3], c.carry2);
    }

    for(; i < size; i ++)
    {
        c.carry1 = _big_int_add_carry1(dest1 + i+0, a1[i+0], b1[i+0], c.carry1);
        c.carry2 = _big_int_add_carry2(dest2 + i+0, a2[i+0], b2[i+0], c.carry2);
    }

    return c;
}

Carries _big_int_sub_conc_carry(
    isize size, Carries carry,
    Limb* dest1, const Limb* a1, const Limb* b1, 
    Limb* dest2, const Limb* a2, const Limb* b2)
{
    Carries c = carry;
    isize i = 0;
    for(; i <= size - 4; i += 4)
    {
        c.carry1 = _big_int_sub_borrow1(dest1 + i+0, a1[i+0], b1[i+0], c.carry1);
        c.carry2 = _big_int_sub_borrow2(dest2 + i+0, a2[i+0], b2[i+0], c.carry2);
        
        c.carry1 = _big_int_sub_borrow1(dest1 + i+1, a1[i+1], b1[i+1], c.carry1);
        c.carry2 = _big_int_sub_borrow2(dest2 + i+1, a2[i+1], b2[i+1], c.carry2);
        
        c.carry1 = _big_int_sub_borrow1(dest1 + i+2, a1[i+2], b1[i+2], c.carry1);
        c.carry2 = _big_int_sub_borrow2(dest2 + i+2, a2[i+2], b2[i+2], c.carry2);
        
        c.carry1 = _big_int_sub_borrow1(dest1 + i+3, a1[i+3], b1[i+3], c.carry1);
        c.carry2 = _big_int_sub_borrow2(dest2 + i+3, a2[i+3], b2[i+3], c.carry2);
    }

    for(; i < size; i ++)
    {
        c.carry1 = _big_int_sub_borrow1(dest1 + i+0, a1[i+0], b1[i+0], c.carry1);
        c.carry2 = _big_int_sub_borrow2(dest2 + i+0, a2[i+0], b2[i+0], c.carry2);
    }

    return c;
}

Carries _big_int_add_conc(isize size,
    Limb* dest1, const Limb* a1, const Limb* b1, 
    Limb* dest2, const Limb* a2, const Limb* b2)
{
    Carries carries = {0};
    return _big_int_add_conc_carry(size, carries, dest1, a1, b1, dest2, a2, b2);
}

Carries _big_int_sub_conc(isize size,
    Limb* dest1, const Limb* a1, const Limb* b1, 
    Limb* dest2, const Limb* a2, const Limb* b2)
{
    Carries carries = {0};
    return _big_int_sub_conc_carry(size, carries, dest1, a1, b1, dest2, a2, b2);
}

enum {KARATSUBA_BASE_CASE = 20};

isize _big_int_mul_karatsuba_calc_size(isize inputs_size)
{
    uint64_t out = 0;
    uint64_t curr = (uint64_t) inputs_size;

    while(curr > KARATSUBA_BASE_CASE)
    {
        uint64_t B = (curr + 1) / 2;
        uint64_t needed = 6*B + 4;

        out += needed;
        curr = curr/2 + 1;
    }

    return out;
}


void _big_int_mul_schoolboy(Limb* out, isize out_size, const Limb* X, isize X_size, const Limb* Y, isize Y_size)
{
    isize needed_size = X_size + Y_size;
    ASSERT(needed_size <= out_size);

    memset(out, 0, needed_size*sizeof(Limb));

    for(isize yi = 0; yi < Y_size; yi ++)
    {
        Limb y = Y[yi];

        u8 carry1 = 0;
        u8 carry2 = 0;
        Limb prev_hi = 0;
        
        isize xi = 0;
        for(; xi <= X_size - 4; xi += 4)
        { 
            Limb hi0 = 0;
            Limb hi1 = 0;
            Limb hi2 = 0;
            Limb hi3 = 0;

            Limb lo0 = _big_int_mul_doubleword(&hi0, X[xi+0], y);
            Limb lo1 = _big_int_mul_doubleword(&hi1, X[xi+1], y);
            Limb lo2 = _big_int_mul_doubleword(&hi2, X[xi+2], y);
            Limb lo3 = _big_int_mul_doubleword(&hi3, X[xi+3], y);

            carry1 = _big_int_add_carry1(&out[xi+0 + yi], out[xi+0 + yi], lo0, carry1);
            carry2 = _big_int_add_carry2(&out[xi+0 + yi], out[xi+0 + yi], prev_hi, carry2);

            carry1 = _big_int_add_carry1(&out[xi+1 + yi], out[xi+1 + yi], lo1, carry1);
            carry2 = _big_int_add_carry2(&out[xi+1 + yi], out[xi+1 + yi], hi0, carry2);
            
            carry1 = _big_int_add_carry1(&out[xi+2 + yi], out[xi+2 + yi], lo2, carry1);
            carry2 = _big_int_add_carry2(&out[xi+2 + yi], out[xi+2 + yi], hi1, carry2);
            
            carry1 = _big_int_add_carry1(&out[xi+3 + yi], out[xi+3 + yi], lo3, carry1);
            carry2 = _big_int_add_carry2(&out[xi+3 + yi], out[xi+3 + yi], hi2, carry2);

            prev_hi = hi3;
        }

        for(; xi < X_size; xi ++)
        { 
            Limb x = X[xi];
            Limb hi = 0;
            Limb lo = _big_int_mul_doubleword(&hi, x, y);

            carry1 = _big_int_add_carry1(&out[xi + yi], out[xi + yi], lo, carry1);
            carry2 = _big_int_add_carry2(&out[xi + yi], out[xi + yi], prev_hi, carry2);

            prev_hi = hi;
        }

        if(yi + X_size < needed_size)
        {
            Limb last = prev_hi + carry1 + carry2;
            out[yi + X_size] = last;
        }
    }
}

__declspec(noinline)
void _big_int_mul_karatsuba_recursive(Limb* out, isize out_size, const Limb* a, const Limb* b, isize ab_size, isize depth, Limb* temp, isize temp_pos, isize temp_cap)
{
    ASSERT(depth < 64);
    if(ab_size < KARATSUBA_BASE_CASE)
    {
        _big_int_mul_schoolboy(out, out_size, a, ab_size, b, ab_size);
    }
    else
    {
        //Xx = X1 + X0
        //Yy = Y1 + Y0
        //Z0 = X0*Y0
        //Z1 = X1*Y1
        //Zm = Xx * Yy - Z0 - Z1 
        //XY = Z0 + Zm*B + Z1*BB

        isize B = (ab_size + 1)/2;
        isize R = ab_size/2;

        isize curr_temp = temp_pos;
        ASSERT(curr_temp <= temp_cap);

        Limb* X0 = a;
        Limb* X1 = a + B;

        Limb* Y0 = b;
        Limb* Y1 = b + B;

        Limb* Z0 = out;                     //2*B + 2*R
        Limb* Z1 = temp + curr_temp; curr_temp += 2*B;
        Limb* Zm = temp + curr_temp; curr_temp += 2*(B + 1);           
        Limb* Xx = temp + curr_temp; curr_temp += B + 1;
        Limb* Yy = temp + curr_temp; curr_temp += B + 1;

        ASSERT(curr_temp <= temp_cap);
        
        //Xx = X0 + X1
        //Yy = Y0 + Y1
        {
            Carries c = _big_int_add_conc(R, 
                Xx, X0, X1, 
                Yy, Y0, Y1);
                
            isize i = R;
            if(R < B)
            {
                c.carry1 = _big_int_add_carry1(Xx + i, X0[i], 0, c.carry1);
                c.carry2 = _big_int_add_carry2(Yy + i, Y0[i], 0, c.carry2);
                i++;
            }

            Xx[i] = c.carry1;
            Yy[i] = c.carry2;
        }
        memset(out + 2*B, 0, 2*R*sizeof(Limb));

        _big_int_mul_karatsuba_recursive(Z0, 2*B,     X0, Y0, B,     depth + 1, temp, curr_temp, temp_cap);                   
        _big_int_mul_karatsuba_recursive(Z1, 2*R,     X1, Y1, R,     depth + 1, temp, curr_temp, temp_cap);                   
        _big_int_mul_karatsuba_recursive(Zm, 2*B + 2, Xx, Yy, B + 1, depth + 1, temp, curr_temp, temp_cap); 
        Carries sub_carries = _big_int_sub_conc(R, 
            Zm, Zm, Z0, 
            Zm, Zm, Z1);
            
        ASSERT(sub_carries.carry1 == 0 && sub_carries.carry1 == 0, 
            "Cannot happen by the nature of the algorithm");

        u8 final_carry1 = _big_int_add_carry(out + B,   out + B,   Zm, 2*B + 2, 0);
        u8 final_carry2 = _big_int_add_carry(out + 2*B, out + 2*B, Z1, 2*R, 0);
        ASSERT(final_carry1 == 0 && final_carry2 == 0, 
            "Cannot happen by the nature of the algorithm");

        //-------B-------2B-------3B-------4B
        //[      Z0      ]
        //       [       Zm          ]
        //               [        Z1        ]
        // 
    }
}

void big_int_sub(Big_Int* out, Big_Int a, Big_Int b)
{
    Big_Int b_neg = b;
    b_neg.sign = (-1)*b.sign;
    big_int_add(out, a, b_neg);
}

int main() 
{
    Limb a[8] = {0};
    Limb b[8] = {0};
    isize a_size = _big_int_assign(a, 0xFFFFFFFF);
    isize b_size = _big_int_assign(b, 0xFFFF);

    Limb res[8] = {0};
    _big_int_mul_schoolboy(res, 8, a, a_size, b, b_size);
    
    u64 res_u64 = 0xFFFFFFFFull*0xFFFFull;
    int x = 0; x = x + 1; x += res_u64 > 0;

        _big_int_mul_karatsuba_recursive(0,0,0,0,0,0,0,0,0);
}

#include <intrin.h>

#ifdef BIG_INT_TESTING
Limb _big_int_mul_doubleword(Limb* high, Limb a, Limb b)
{
    u64 product = (u64) a * (u64) b;
    *high = (Limb) (product >> sizeof(Limb)*8);
    return (Limb) product;
}

u8 _big_int_add_carry1(Limb* out, Limb a, Limb b, u8 carry)
{
    return _addcarry_u8(carry, a, b, out);
}
u8 _big_int_sub_borrow1(Limb* out, Limb a, Limb b, u8 carry)
{
    return _subborrow_u8(carry, a, b, out);
}

u8 _big_int_add_carry2(Limb* out, Limb a, Limb b, u8 carry)
{
    return _addcarry_u8(carry, a, b, out);
}
u8 _big_int_sub_borrow2(Limb* out, Limb a, Limb b, u8 carry)
{
    return _subborrow_u8(carry, a, b, out);
}

#else
Limb _big_int_mul_doubleword(Limb* high, Limb a, Limb b)
{
    return _mul128(a, b, high);
}

u8 _big_int_add_carry1(Limb* out, Limb a, Limb b, u8 carry)
{
    return _addcarry_u64(carry, a, b, out);
}

u8 _big_int_sub_borrow1(Limb* out, Limb a, Limb b, u8 carry)
{
    return _subborrow_u64(carry, a, b, out);
}

u8 _big_int_add_carry2(Limb* out, Limb a, Limb b, u8 carry)
{
    return _addcarryx_u64(carry, a, b, out);
}

u8 _big_int_sub_borrow2(Limb* out, Limb a, Limb b, u8 carry)
{
    return _subborrow_u64(carry, a, b, out);
}
#endif