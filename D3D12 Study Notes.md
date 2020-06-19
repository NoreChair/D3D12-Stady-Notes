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

到这里为止，我们已经创建了 DX12 所必备的几个环境，接下来我们会继续研究 `RenderTarget` `DepthStencil` `Vertex/Index Buffer` `Const Buffer` `Root Signature` `Shader` 和 `PipelineState`

## 渲染的状态与Buffer

### ID3D12Resource

### DescriptorHeap 和 BufferView

### Render Pipeline

## 示例：绘制一个三角

* 你可以在 Sample/Triangle 下参考如何绘制一个三角形

## 调试

* 比较简单的方式是使用 [NSight Visual Studio Edition](https://developer.nvidia.com/nsight-visual-studio-edition-2019_4) Debug 你的 DX12 程序，需要注意的一点是最新的 2020 版本已经移除了 Graphic Debug 。
