// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphActionNode.h"

#include <Schematyc/Compiler/CompilerContext.h>
#include <Schematyc/Compiler/IGraphNodeCompiler.h>
#include <Schematyc/Env/EnvUtils.h>
#include <Schematyc/Env/IEnvRegistry.h>
#include <Schematyc/Env/Elements/IEnvAction.h>
#include <Schematyc/Env/Elements/IEnvSignal.h>
#include <Schematyc/Reflection/ActionDesc.h>
#include <Schematyc/Utils/IGUIDRemapper.h>
#include <Schematyc/Utils/StackString.h>

#include "Object.h"
#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{

CScriptGraphActionNode::SRuntimeData::SRuntimeData(uint32 _actionIdx)
	: actionIdx(_actionIdx)
{}

CScriptGraphActionNode::SRuntimeData::SRuntimeData(const SRuntimeData& rhs)
	: actionIdx(rhs.actionIdx)
{}

void CScriptGraphActionNode::SRuntimeData::ReflectType(CTypeDesc<CScriptGraphActionNode::SRuntimeData>& desc)
{
	desc.SetGUID("691015af-4d45-4644-8214-b984ab860dd2"_schematyc_guid);
}

CScriptGraphActionNode::CScriptGraphActionNode() {}

CScriptGraphActionNode::CScriptGraphActionNode(const SGUID& actionTypeGUID)
	: m_actionTypeGUID(actionTypeGUID)
{}

SGUID CScriptGraphActionNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphActionNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetStyleId("Core::Function"); // #SchematycTODO : Actions should have their own unique style!!!

	layout.AddInput("Start", SGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::End });

	const char* szSubject = nullptr;
	const IEnvAction* pEnvAction = gEnv->pSchematyc->GetEnvRegistry().GetAction(m_actionTypeGUID);
	if (pEnvAction)
	{
		szSubject = pEnvAction->GetName();

		const CActionDesc& actionDesc = pEnvAction->GetDesc();
		for (const CClassMemberDesc& actionMemberDesc : actionDesc.GetMembers())
		{
			const void* pDefaultValue = actionMemberDesc.GetDefaultValue();
			if (pDefaultValue)
			{
				const CCommonTypeDesc& actionMemberTypeDesc = actionMemberDesc.GetTypeDesc();
				layout.AddInputWithData(CGraphPortId::FromUniqueId(actionMemberDesc.GetId()), actionMemberDesc.GetLabel(), actionMemberTypeDesc.GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, CAnyConstRef(actionMemberTypeDesc, pDefaultValue));
			}
		}

		auto visitEnvSignal = [&layout](const IEnvSignal& envSignal)
		{
			layout.AddOutput(CGraphPortId::FromGUID(envSignal.GetGUID()), envSignal.GetName(), envSignal.GetGUID(), { EScriptGraphPortFlags::Signal, EScriptGraphPortFlags::Begin });
		};
		EnvUtils::VisitChildren(*pEnvAction, CDelegate<void(const IEnvSignal&)>::FromLambda(visitEnvSignal));
	}
	layout.SetName(nullptr, szSubject);
}

void CScriptGraphActionNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	CRuntimeClass* pClass = context.interfaces.Query<CRuntimeClass>();
	if (pClass)
	{
		const IEnvAction* pEnvAction = gEnv->pSchematyc->GetEnvRegistry().GetAction(m_actionTypeGUID);
		if (pEnvAction)
		{
			const SGUID guid = CScriptGraphNodeModel::GetNode().GetGUID();

			const IScriptElement* pOwner = CScriptGraphNodeModel::GetNode().GetGraph().GetElement().GetParent();
			switch (pOwner->GetType())
			{
			case EScriptElementType::Class:
				{
					const uint32 actionIdx = pClass->AddAction(guid, m_actionTypeGUID);
					compiler.BindCallback(&Execute);
					compiler.BindData(SRuntimeData(actionIdx));

					uint32 outputIdx = 0;

					auto visitEnvSignal = [&compiler, pClass, &guid, &outputIdx](const IEnvSignal& envSignal)
					{
						pClass->AddSignalReceiver(envSignal.GetGUID(), guid, compiler.GetGraphIdx(), SRuntimeActivationParams(compiler.GetGraphNodeIdx(), outputIdx++, EActivationMode::Output));
					};
					EnvUtils::VisitChildren(*pEnvAction, CDelegate<void(const IEnvSignal&)>::FromLambda(visitEnvSignal));
					break;
				}
			case EScriptElementType::State:
				{
					const uint32 stateIdx = pClass->FindState(pOwner->GetGUID());
					if (stateIdx != InvalidIdx)
					{
						const uint32 actionIdx = pClass->AddStateAction(stateIdx, guid, m_actionTypeGUID);
						compiler.BindCallback(&Execute);
						compiler.BindData(SRuntimeData(actionIdx));

						uint32 outputIdx = 0;

						auto visitEnvSignal = [&compiler, pClass, stateIdx, &guid, &outputIdx](const IEnvSignal& envSignal)
						{
							pClass->AddStateSignalReceiver(stateIdx, envSignal.GetGUID(), guid, compiler.GetGraphIdx(), SRuntimeActivationParams(compiler.GetGraphNodeIdx(), outputIdx++, EActivationMode::Output));
						};
						EnvUtils::VisitChildren(*pEnvAction, CDelegate<void(const IEnvSignal&)>::FromLambda(visitEnvSignal));
					}
					else
					{
						SCHEMATYC_COMPILER_ERROR("Failed to retrieve class state!");
					}
					break;
				}
			}

			
		}
	}
}

void CScriptGraphActionNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_actionTypeGUID, "actionTypeGUID");
}

void CScriptGraphActionNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_actionTypeGUID, "actionTypeGUID");
}

void CScriptGraphActionNode::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	Validate(archive, context);
}

void CScriptGraphActionNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context) {}

void CScriptGraphActionNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			CCreationCommand(const char* szSubject, const SGUID& actionTypeGUID)
				: m_subject(szSubject)
				, m_actionTypeGUID(actionTypeGUID)
			{}

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Action";
			}

			virtual const char* GetSubject() const override
			{
				return m_subject.c_str();
			}

			virtual const char* GetDescription() const override
			{
				return nullptr;
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Action";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphActionNode>(m_actionTypeGUID), pos);
			}

			// ~IScriptGraphNodeCreationCommand

		private:

			string m_subject;
			SGUID  m_actionTypeGUID;
		};

	public:

		// IScriptGraphNodeCreator

		virtual SGUID GetTypeGUID() const override
		{
			return CScriptGraphActionNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const SGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphActionNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			if (!gEnv->pSchematyc->IsExperimentalFeatureEnabled("ActionNode"))
			{
				return;
			}

			switch (graph.GetType())
			{
			case EScriptGraphType::Signal:
				{
					nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>("Timer", "6937eddc-f25c-44dc-a759-501d2e5da0df"_schematyc_guid));
					nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>("DebugText", "8ea4441b-e080-4cca-8b3e-973e017404d3"_schematyc_guid));
					break;
				}
			}
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphActionNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	if (activationParams.IsInput(EInputIdx::Start))
	{
		const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
		CObject* pObject = static_cast<CObject*>(context.pObject);

		StackRuntimeParams params;
		for (uint8 inputIdx = EInputIdx::FirstParam, inputCount = context.node.GetInputCount(); inputIdx < inputCount; ++inputIdx)
		{
			if (context.node.IsDataInput(inputIdx))
			{
				params.SetInput(context.node.GetInputId(inputIdx).AsUniqueId(), *context.node.GetInputData(inputIdx));
			}
		}
		
		pObject->StartAction(data.actionIdx, params);

		return SRuntimeResult(ERuntimeStatus::Break);
	}
	else
	{
		return SRuntimeResult(ERuntimeStatus::Continue, activationParams.portIdx);
	}
}

const SGUID CScriptGraphActionNode::ms_typeGUID = "4bb42a1b-6268-4c7a-9c13-37192f874cb1"_schematyc_guid;

} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphActionNode::Register)
