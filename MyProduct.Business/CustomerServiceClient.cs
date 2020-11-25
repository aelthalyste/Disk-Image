using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.ServiceModel.Description;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Business
{
    public class CustomerServiceClient : ClientBase<ICustomerService>
    {
        public CustomerServiceClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(ICustomerService)),
                new NetNamedPipeBinding(),
                new EndpointAddress("net.pipe://localhost/myproduct/customerservice")))
        {
            
        }

        public ICustomerService CustomerService { get => Channel; }
    }
}
