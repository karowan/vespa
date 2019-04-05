package com.yahoo.vespa.hosted.controller.api.role;

import com.yahoo.config.provision.SystemName;

/**
 * A {@link Role} with a {@link Context} of only a {@link SystemName}.
 *
 * @author jonmv
 */
public class UnboundRole extends Role {

    UnboundRole(RoleDefinition roleDefinition, SystemName system) {
        super(roleDefinition, Context.unlimitedIn(system));
    }

    @Override
    public String toString() {
        return "role '" + definition() + "'";
    }

}
