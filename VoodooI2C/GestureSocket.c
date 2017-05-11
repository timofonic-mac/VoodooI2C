//
//  GestureSocket.c
//  VoodooI2C
//
//  Copyright © 2017 Kishor Prins. All rights reserved.
//

#include <libkern/OSMalloc.h>
#include "GestureSocket.h"

// handler for incoming connections from user space clients
static errno_t ctl_connect(kern_ctl_ref ctl_ref, struct sockaddr_ctl* sac, void** unitinfo);
// handler for disconnecting user space clients
static errno_t ctl_disconnect(kern_ctl_ref ctl_ref, u_int32_t unit, void* unitinfo);

// control structure needed
static struct kern_ctl_reg ctl_reg = {
    GESTURE_CLIENT_CTL_NAME,             // socket identifier
    0,                                   // flags
    0,                                   // ctl_unit
    0,                                   // privileged access
    0,                                   // send buffer size
    0,                                   // recv size buffer
    ctl_connect,                         // connected
    ctl_disconnect,                      // disconnected
    NULL,                                // ctl_send_func
    NULL,                                // setsockopt call
    NULL                                 // getsockopt call
};

static kern_ctl_ref ctl_ref = NULL; // reference to control structure
static OSMallocTag malloc_tag = NULL; // malloc tag
static lck_grp_t* lock_group = NULL; // lock group
static lck_mtx_t* lock = NULL; // concruency management

static kern_ctl_ref current_connection = NULL; // refernce to the currently connected client
static u_int32_t current_unit = 0; // unit number for the currently connected client

errno_t ctl_connect(kern_ctl_ref ctl_ref, struct sockaddr_ctl* sac, void** unitinfo) {
    lck_mtx_lock(lock);
    
    current_connection = ctl_ref;
    current_unit = sac->sc_unit;
    
    lck_mtx_unlock(lock);
    
    return KERN_SUCCESS;
}

errno_t ctl_disconnect(kern_ctl_ref ctl_ref, u_int32_t unit, void* unitinfo) {
    lck_mtx_lock(lock);
    
    if(unit != current_unit) {
        // invalid unit number (different client disconnecting?)
        lck_mtx_unlock(lock);
        return KERN_SUCCESS;
    }
    
    current_connection = NULL;
    current_unit = 0;
    
    lck_mtx_unlock(lock);
    
    return KERN_SUCCESS;
}


kern_return_t initialise_gesture_socket() {
    // allocate memory for the locks
    malloc_tag = OSMalloc_Tagalloc(GESTURE_CLIENT_CTL_NAME, OSMT_DEFAULT);
    
    // sanity check
    if(malloc_tag == NULL) {
        return KERN_NO_SPACE;
    } else {
         lock_group = lck_grp_alloc_init(GESTURE_CLIENT_CTL_NAME, LCK_GRP_ATTR_NULL);
    }
    
    if(lock_group != NULL) {
        lock = lck_mtx_alloc_init(lock_group, LCK_ATTR_NULL);
    } else {
        // free memory for malloc_tag
        OSMalloc_Tagfree(malloc_tag);
        malloc_tag = NULL;
        return KERN_NO_SPACE;
    }
    
    // free allocated memory for malloc_tag and lock_group
    if(lock == NULL) {
        lck_grp_free(lock_group);
        lock_group = NULL;
        
        OSMalloc_Tagfree(malloc_tag);
        malloc_tag = NULL;
        
        return KERN_NO_SPACE;
    }
    
    errno_t return_status = ctl_register(&ctl_reg, &ctl_ref);

    if(return_status != KERN_SUCCESS) {
        // failed to register the control structure
        return KERN_FAILURE;
    }

    return KERN_SUCCESS;
}

kern_return_t destroy_gesture_socket() {
    // never registered the control structure
    if(ctl_ref == NULL) {
        return KERN_SUCCESS;
    }
    
    errno_t return_status = ctl_deregister(ctl_ref);
    
    if(return_status != 0) {
        ctl_ref = NULL;
    } else {
        // clients are still connected on the socket
        // if we do not allow clients to close connection properly then KP will occur
        return KERN_FAILURE;
    }
    
    if(lock) {
        lck_mtx_free(lock, lock_group);
        lock = NULL;
    }
    
    if(lock_group) {
        lck_grp_free(lock_group);
        lock_group = NULL;
    }
    
    if (malloc_tag) {
        OSMalloc_Tagfree(malloc_tag);
        malloc_tag = NULL;
    }
    
    return KERN_SUCCESS;
}