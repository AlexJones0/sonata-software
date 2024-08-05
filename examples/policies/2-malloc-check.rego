package malloc_check

import future.keywords

is_allocator_capability(capability) {
    capability.kind == "SealedObject"
    capability.sealing_type.compartment == "alloc"
    capability.sealing_type.key == "MallocKey"
}

decode_allocator_capability(capability) = decoded_info {
    is_allocator_capability(capability)
    some quota
    quota = integer_from_hex_string(capability.contents, 0, 4)
    every x in [4, 8, 12, 16, 20] {
        integer_from_hex_string(capability.contents, x, 4) == 0
    }
    decoded_info := { "quota": quota }
}

all_sealed_allocator_capabilities_are_valid {
    some allocatorCapabilities
    allocatorCapabilities = [ c | c = input.compartments[_].imports[_] ; is_allocator_capability(c) ]
    every c in allocatorCapabilities {
        decode_allocator_capability(c)
    }
}

allocator_capabilities := [
    {"owner": owner, "capability": data.rtos.decode_allocator_capability(cap)} | 
    cap = input.compartments[owner].imports[_] ; 
    data.rtos.is_allocator_capability(cap)
]

all_allocations_leq(limit) {
    every cap in allocator_capabilities {
        cap.capability.quota <= limit
    }
}

unique_allocator_compartments contains owner if {
    some {"owner": owner} in allocator_capabilities
}

total_quota_per_compartment[owner] = quota if {
    some owner in unique_allocator_compartments
    quota := sum([cap.capability.quota | cap = allocator_capabilities[_]; cap.owner == owner])
}

all_compartments_allocate_leq(limit) {
    some quotas
    quotas = [ quota | quota = total_quota_per_compartment[_] ]
    every quota in quotas {
        quota <= limit
    }
}

total_quota := sum([cap.capability.quota | cap = allocator_capabilities[_]])

total_allocation_leq(limit) {
    total_quota <= limit
}

default valid := false
valid {
    all_sealed_allocator_capabilities_are_valid
    # No individual allocation should be able to allocate more than 20 KiB at one time
    all_allocations_leq(20480)
    # No individual compartment should be able to allocate more than 40 KiB simultaneously
    all_compartments_allocate_leq(40960) 
    # The entire firmware image cannot allocate more than 100 KiB simultaneously
    total_allocation_leq(102400)
}
