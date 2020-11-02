"""Two 10-core servers connected by ConnectX-4 NIC for running AIFM."""

# Import the Portal object.
import geni.portal as portal
# Import the ProtoGENI library.
import geni.rspec.pg as pg
# Import the Emulab specific extensions.
import geni.rspec.emulab as emulab

# Describe the parameter(s) this profile script can accept.
portal.context.defineParameter( "n", "Number of Machines", portal.ParameterType.INTEGER, 2 )

# Retrieve the values the user specifies during instantiation.
params = portal.context.bindParameters()

# Create a portal object,
pc = portal.Context()

# Create a Request object to start building the RSpec.
request = pc.makeRequestRSpec()

nodes = []
ifaces = []

# nodes.append(node_0)
# ifaces.append(iface0)

for i in range(0, params.n):
    n = request.RawPC('node-%d' % i)
    n.routable_control_ip = True
    n.hardware_type = 'xl170'
    n.disk_image = 'urn:publicid:IDN+emulab.net+image+emulab-ops//UBUNTU18-64-STD'

    intf = n.addInterface('interface-%d' % i, pg.IPv4Address('192.168.6.%d' % (i + 1),'255.255.255.0'))
    nodes.append(n)
    ifaces.append(intf)


# Link link-0
link_0 = request.LAN('link-0')
link_0.best_effort = False
link_0.bandwidth = 25000000
link_0.setNoInterSwitchLinks()
link_0.Site('undefined')
for iface in ifaces:
    link_0.addInterface(iface)

# Print the generated rspec
pc.printRequestRSpec(request)
