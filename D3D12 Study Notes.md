# D3D12 Study Notes

## 初始化环境

### Adapter && Device

* 要找到你需要建立 dx12 环境的 Adapter，首先你要创建一个 IDXGIFactory ，通过 factory 你可以遍历所有的适配器（显卡或者软驱）和与适配器相链接的显示器。获取到你想要的 Adapter 就可以创建对应的 Device 。代码如下:

  ```cpp
  // ComPtr 相当于是智能指针,在无引用的情况下会自动去调用 Release
  // 所以你并不需要手动调用 Release，只需要将此设为 nullptr 即可
  ComPtr<IDXGIFactory4> dxgiFactory;
  ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&dxgiFactory)));
  // 获取你的硬件设备
  GetHardwareAdapter(dxgiFactory.Get(), &m_adapter);
  // 创建的你的 Device， IID_PPV_ARGS 宏用于获取指针对象的类型 uuid 并将指针重解释为 void**
  ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
  ```

### CommandQueue CommandAllocator 和 CommandList

* CommandQueue 是 DX12 里的一个新概念，一般情况下只会存在一个，他与 CommandList 相结合可以用于多线程渲染。CommandQueue 并不能在多线程中并行处理，但是每个线程可以持有自己的 CommandList，线程将绘制命令填充在 CommandList 中， 最后在一个同步点上由 CommandQueue 去 Execute。
* CommandList 的创建需要 CommandAllocator 来给他具体的分配内存。每当 CommandList 填充完毕需要 Execute 时，都需要调用 Close() 来手动关闭才可以被 CommandQueue 执行。然后由 CommandAllocator 进行重新的内存分配。
* 需要注意的是 CommandList 只有当被 Execute 的时候才是将命令提交到 GPU 中，和之前的单条命令改变状态机是一样的。

  CommandQueue CommandAllocator 和 CommandList 代码如下：

  ```cpp
  // Create Command Queue
  D3D12_COMMAND_QUEUE_DESC  desc = { };
  desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));

  // Create CommandAllocator And CommandList
  ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
  ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
  // we don't use this command list in initialization,so close it now
  m_commandList->Close();

  // ---------------------------------Rendering--------------------------------------//
  // clear all command in previous frame so we can begin new frame
  ThrowIfFailed(m_commandAllocator->Reset());
  // allocate cache
  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
  ```

### SwapChain

* SwapChain 就不过多解释了，就是常用的双重缓冲或者三重缓冲（取决于你设置的 BufferCount）。与之前有所不同的是 SwapChain 现在需要我们手动设置 Fence 。

  ```cpp
  // ComPtr<IDXGISwapChain3> m_swapChain;
  ComPtr<IDXGISwapChain1> swapChain;
  DXGI_SWAP_CHAIN_DESC1 desc = { };
  desc.BufferCount = s_frameCount;
  desc.Width = m_width;
  desc.Height = m_height;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  desc.SampleDesc.Count = 1;

  ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), Win32Application::GetHwnd(), &desc, nullptr, nullptr, &swapChain));
        ThrowIfFailed(swapChain.As(&m_swapChain));

  // Get current frame index in swapchain
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
  // Create fence and event
  ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceValue = 1;
  m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);

  // ---------------------------------Rendering--------------------------------------//
  // do draw command.......
  // and in the end
  ThrowIfFailed(m_swapChain->Present(1, 0));
  const UINT64 curFence = m_fenceValue;
  // set synchronized signal
  ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), curFence));
  m_fenceValue++;

  if (m_fence->GetCompletedValue() < curFence) {
      // if fence not completed last synchronized signal,then wait for completion
      ThrowIfFailed(m_fence->SetEventOnCompletion(curFence, m_fenceEvent));
      // not the best practice but enough in this example
      WaitForSingleObject(m_fenceEvent, INFINITE);
  }
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
  ```

到这里为止，我们已经创建了 DX12 所必备的几个环境，接下来我们会继续研究 ：

* `Render Target`
* `Depth Stencil`
* `Vertex/Index Buffer`
* `Const Buffer`
* `Root Signature`
* `Shader Program`
* `Pipeline State`

## 渲染的状态与Buffer

### ID3D12Resource

### DescriptorHeap 和 BufferView

#### Root Signature

* 从概念上来讲 Root Signature 更像是函数的形参，我们每一个 Shader Program 都有固定的输入参数，包括 CBV/SRV/UAV。但是只有在运行时我们才将真正的数据设置进去，Root Signature 就是 D3D12 用来定义这些输入参数的合集。
* 说一个 Root Signature 是输入参数的和合集，从其主要参数是 `Root Parameter[]` 就可以明白。Root Parameter 可以分为三种 ： `Descriptor Table` / `Root Descriptor` / `Root Constants`。一个 Root Signature 就是由这三种参数组成的集合，与 Shader Program 组合以确定输入的位置。
* 需要注意的一点是， Root Signature 只是定义了输入的形参，并不实际和 Shader Program 有关联，只有在 PipelineState 中才会进行关联。所以如果不同 Shader 的布局能够兼容，Root Signature 可以只有一个。另外切换一个 Root Signature 是开销很大的。
* 为了性能考虑，单个 Root Signature 只可以设置 64 DWORDs 大小的。不同类型的 Parameter 占不同大小的
  * Descriptor Table : 1 DWORD
  * Root Descriptor : 2 DWORDs
  * Root Constants : 1 DWORD per 32_bit constant
* 以下为如何创建 Root Signature 的示例

```cpp
// set root parameter
CD3DX12_ROOT_PARAMETER slotRootParameter[1];
// parameter init as descriptor table
CD3DX12_DESCRIPTOR_RANGE cbvTable;
cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

// create root signature which is array of parameter
CD3DX12_ROOT_SIGNATURE_DESC signature_desc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
ComPtr<ID3DBlob> serializedRootSig = nullptr;
ComPtr<ID3DBlob> errorBlob = nullptr;
HRESULT hr = D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
ThrowIfFailed(m_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
```

* 接下来我们重点解释下 Root Parameter 的三种形式以及如何设置我们 Shader 需要的实参。

##### Descriptor Table

* 我们之前讲过 Descriptor Heap，而 Descriptor Table 类型的 Parameter 指代其在 Descriptor Heap 上的一段连续域。我们可以看一下如果我们 ParameterType 选为 Descriptor Table 那我们就需要填充一下参数。

  ```cpp
  typedef struct D3D12_ROOT_DESCRIPTOR_TABLE
  {
  UINT NumDescriptorRanges;
  _Field_size_full_(NumDescriptorRanges)  const D3D12_DESCRIPTOR_RANGE *pDescriptorRanges;
  } D3D12_ROOT_DESCRIPTOR_TABLE;
  ```

  NumDescriptorRanges 指定了我们总共有多少个 BufferView 需要被设置。 pDescriptorRanges 分别指定了是什么种类的 BufferView(CBV/SRV/UAV)，在 Shader 中又是从哪个寄存器开始，例如指定 CBV Type 并且设置 baseRegister 为0，在 HLSL 中就是 `(cBuffer cbA : register(b0))`。

* 以下代码示例创建 3 个 CBV, 2 个 SRV , 1 个 UAV

  ```cpp
  CD3DX12_ROOT_PARAMETER slotRootParameter[1];
  CD3DX12_DESCRIPTOR_RANGE descRanges[3];
  // D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND 会自动帮我们定位偏移索引，如果将此宏去掉用手动的方式设置，分别是 0/3/5
  descRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,3,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
  descRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
  descRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
  slotRootParameter[0].InitAsDescriptorTable(3, descRanges);

  // ---------------------------------Rendering--------------------------------------//
  m_commandList->SetGraphicsRootDescriptorTable(0,baseDescriptorHandle); // baseDescriptor is CBV Handle then 2 CBV 2 SRV 1 UAV
  ```

##### Root Descriptor

##### Root Constants

### Render Pipeline

## 示例：绘制一个三角

* 你可以在 Sample/Triangle 下参考如何绘制一个三角形

## 调试

* 比较简单的方式是使用 [NSight Visual Studio Edition](https://developer.nvidia.com/nsight-visual-studio-edition-2019_4) Debug 你的 DX12 程序，需要注意的一点是最新的 2020 版本已经移除了 Graphic Debug 。
