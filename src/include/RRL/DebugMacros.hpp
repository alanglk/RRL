#pragma once

/**
 * @brief These macros will be stripped out on release mode.
 *  They are useful to perform some assertions at debugging 
 *  while ensuring speed at realease.
 */



#ifndef NDEBUG
#include <FLogging/FLogging.hpp>

    /**
     * @brief Assert an entity has a certain component
     */
    #define RRL_ASSERT_HAS_COMPONENT(reg, ent, comp_type, msg) \
        if (!reg.all_of<comp_type>(ent)) { \
            LOG_ERROR(msg); \
            std::abort(); \
        }
    /**
     * @brief Assert an entity does not have a certain component
     */
    #define RRL_ASSERT_NOT_HAS_COMPONENT(reg, ent, comp_type, msg) \
        if (reg.all_of<comp_type>(ent)) { \
            LOG_ERROR(msg); \
            std::abort(); \
        }


#else

    #define RRL_ASSERT_HAS_COMPONENT(reg, ent, comp_type, msg) ((void)0)
    #define RRL_ASSERT_NOT_HAS_COMPONENT(reg, ent, comp_type, msg) ((void)0)


#endif