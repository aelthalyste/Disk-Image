using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.ServiceModel.Description;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Business
{
    public class ProductServiceClient : ClientBase<IProductService>
    {
        public ProductServiceClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(IProductService)),
                new NetNamedPipeBinding(),
                new EndpointAddress("net.pipe://localhost/myproduct/productservice")))
        {

        }
        public IProductService ProductService { get => Channel; }
    }
}
