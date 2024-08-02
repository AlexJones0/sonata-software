package no_seal

is_sealed_capability(capability) {
    capability.kind == "SealedObject"
}

no_sealed_capabilities_exist {
    some sealedCapabilities
    sealedCapabilities = [ cap | cap = input.compartments[_].imports[_] ; is_sealed_capability(cap) ]
    count(sealedCapabilities) == 0
}

sealed_capability_info := [ 
    {"contents": cap.contents, 
     "key": cap.sealing_type.key, 
     "compartment": cap.sealing_type.compartment, 
    } | cap = input.compartments[owner].imports[_] 
    ; data.no_seal.is_sealed_capability(cap)
]

default valid = false
valid = true {
    no_sealed_capabilities_exist
}
