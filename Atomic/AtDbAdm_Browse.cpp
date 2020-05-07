#include "AtIncludes.h"
#include "AtDbAdm.h"

#include "AtJsonPretty.h"
#include "AtScripts.h"
#include "AtStrFmt.h"


namespace
{
	char const* const c_zCmdSaveEntity   = "Save entity";
	char const* const c_zCmdImportScript = "Import script";
	char const* const c_zCmdRemoveEntity = "Remove this entity and all children";
}


namespace At
{
	ReqResult DbAdm_Browse::WHP_Process(EntityStore& store, HttpRequest& req)
	{
		Seq entityIdStr { req.QueryNvp("entityId") };
		ObjId entityId;
		if (!entityId.ReadStr(entityIdStr))
			entityId = ObjId::Root;

		m_entity = store.GetEntity(entityId, ObjId::None, EntityKind::Unknown);
		if (m_entity.Any())
		{
			if (req.IsPost())
			{
				Seq cmd { req.PostNvp("cmd").Trim() };

				if (cmd == c_zCmdSaveEntity)
				{
					m_editedEntityJson = req.PostNvp("entityJson");
					ParseTree pt { m_editedEntityJson };
					if (!pt.Parse(Json::C_Json))
						AddPageErr(Str("Entity JSON does not match grammar: ").Obj(pt, ParseTree::BestAttempt));
					else
					{
						ParseNode const* parseNode = pt.Root().FlatFind(Json::id_Object);
						if (!parseNode)
							AddPageErr("Entity JSON is not an object");
						else
						{
							Str prevKey;
							m_entity->EncodeKey(prevKey);

							bool decoded {};
							try { m_entity->JsonDecode(*parseNode, nullptr); decoded = true; }
							catch (Json::DecodeErr const& e) { AddPageErr(Str("Error decoding entity JSON: ").Add(e.what())); }

							if (decoded)
							{
								bool dupKey {};
								if (m_entity->UniqueKey() && m_entity->m_parentId.Any())
								{
									Str newKey;
									m_entity->EncodeKey(newKey);

									if (newKey != prevKey && store.ChildWithSameKeyExists(m_entity->m_parentId, m_entity.Ref()))
										dupKey = true;
								}

								if (dupKey)
									AddPageErr("Entity key must be unique and parent already contains a child with the same key");
								else
								{
									m_entity->Update();

									SetRedirectResponse(HttpStatus::SeeOther, Str(DAP_Env_MountPath()).Add("browse?entityId=").UrlEncode(Str::From(entityId)));
									return ReqResult::Done;
								}
							}
						}
					}
				}
				else if (cmd == c_zCmdImportScript)
				{
					Opt<Mime::PartContent> content;
					req.ForEachFileInput([&] (Mime::Part const& part, Seq) -> bool
						{ content.Init(part, req.GetPinStore()); return false; } );

					     if (!content.Any())        AddPageErr("No import script found in request");
					else if (!content->m_success)   AddPageErr("Error decoding import script");
					else if (!content->m_decoded.n) AddPageErr("Import script is empty");
					else
					{
						ProcessImportScript(store, content->m_decoded);
						if (AnyPageErrs())
							SetAbortTxOnSuccess();
					}
				}
				else if (cmd == c_zCmdRemoveEntity)
				{
					m_entity->RemoveChildren();
					m_entity->Remove();

					SetRedirectResponse(HttpStatus::SeeOther, Str(DAP_Env_MountPath()).Add("browse?entityId=").UrlEncode(Str::From(m_entity->m_parentId)));
					return ReqResult::Done;
				}
				else
					return ReqResult::BadRequest;
			}

			store.EnumAllChildren(m_entity->m_entityId,
				[&] (EntityChildInfo const& eci) -> bool
					{ m_children.Add(eci); return true; } );
		}

		return ReqResult::Continue;
	}


	void DbAdm_Browse::ProcessImportScript(EntityStore& store, Seq content)
	{
		Entity::JsonIds jsonIds;
		jsonIds.Add(Entity::JsonId { ".", m_entity->m_entityId });

		ParseTree pt { content };
		if (!pt.Parse(Json::C_Json))
			AddPageErr(Str("Import script does not match JSON grammar: ").Obj(pt, ParseTree::BestAttempt));
		else
		{
			ParseNode const* parseNode = pt.Root().FlatFind(Json::id_Array);
			if (!parseNode)
				AddPageErr("Import script must be a JSON array containing instructions");
			else
			{
				for (ParseNode const& childNode : *parseNode)
					if (childNode.IsType(Json::id_Value))
					{
						if (!childNode.IsType(Json::id_Object))
							AddPageErr(Str::From(childNode, ParseNode::TagRowCol).Add(": Expecting JSON object representing an instruction"));
						else
						{
							try
							{
								enum InstructionState { ExpectInstruction, ExpectJsonIdOpt, ExpectEntity, ExpectEnd };
								InstructionState state { ExpectInstruction };

								enum InstructionType { InstrUnknown, InstrFind, InstrRemove, InstrInsert };
								InstructionType instructionType;

								Str jsonId;
								Rp<Entity> e;

								Json::DecodeObject(childNode, [&] (ParseNode const& nameParser, Seq name, ParseNode const& valueParser) -> bool
									{
										if (state == ExpectInstruction)
										{
											if (!name.EqualExact("i"))
												throw Json::DecodeErr(nameParser, "Expected instruction 'i'");

											Str instructionStr;
											Json::DecodeString(valueParser, instructionStr);

											Seq instructionSeq = instructionStr;
											     if (instructionSeq.EqualExact("find"   )) instructionType = InstrFind;
											else if (instructionSeq.EqualExact("remove" )) instructionType = InstrRemove;
											else if (instructionSeq.EqualExact("insert" )) instructionType = InstrInsert;
											else
												throw Json::DecodeErr(valueParser, "Unrecognized instruction");

											state = ExpectJsonIdOpt;
										}
										else if (state == ExpectJsonIdOpt)
										{
											if (!name.EqualExact("j"))
											{
												if (instructionType == InstrInsert)
												{
													if (name.EqualExact("e"))
														goto DecodeEntity;
													else
														throw Json::DecodeErr(nameParser, "Expected JSON ID 'j' or entity 'e'");
												}

												throw Json::DecodeErr(nameParser, "Expected JSON ID 'j'");
											}

											Json::DecodeString(valueParser, jsonId);
											state = ExpectEntity;
										}
										else if (state == ExpectEntity)
										{
										DecodeEntity:
											if (!name.EqualExact("e"))
												throw Json::DecodeErr(nameParser, "Expected entity 'e'");

											Entity::JsonDecodeDynEntity(valueParser, e, &store, &jsonIds);
											state = ExpectEnd;
										}
										else
											throw Json::DecodeErr(nameParser, "Expected end of JSON object");

										return true;
									} );

								if (instructionType == InstrFind)
								{
									if (!jsonId.Any())
										throw Json::DecodeErr(childNode, "Find instruction requires JSON ID 'j' to which to assign the found entity ID");

									ObjId entityId = store.FindChildIdWithSameKey(e->m_parentId, e.Ref());
									jsonIds.Add(Entity::JsonId { jsonId, entityId });
								}
								else if (instructionType == InstrRemove)
								{
									if (!jsonId.Any())
										throw Json::DecodeErr(childNode, "Remove instruction requires JSON ID 'j' of entity to remove");

									Entity::JsonIds::ConstIt it = jsonIds.Find(jsonId);
									if (!it.Any())
										throw Json::DecodeErr(childNode, "Could not find the specified entity JSON ID");

									if (it->m_objId.Any())
									{
										store.RemoveEntityChildren(it->m_objId);
										store.RemoveEntity(it->m_objId);
										++m_nrEntitiesRemoved;
									}
								}
								else if (instructionType == InstrInsert)
								{
									if (e->UniqueKey() && store.ChildWithSameKeyExists(e->m_parentId, e.Ref()))
										throw Json::DecodeErr(childNode, "Inserting this entity would violate a unique key constraint");
						
									e->Insert_ParentExists();
									++m_nrEntitiesInserted;

									if (jsonId.Any())
										jsonIds.Add(Entity::JsonId { jsonId, e->m_entityId });
								}
								else
									EnsureThrow(!"Unrecognized instruction type");
							}
							catch (Json::DecodeErr const& e)
							{
								AddPageErr(Str::From(childNode, ParseNode::TagRowCol).Add(": Error decoding import script instruction: ").Add(e.what()));
								break;
							}
						}
					}
			}
		}
	}


	HtmlBuilder& DbAdm_Browse::WHP_Body(HtmlBuilder& html, HttpRequest&) const
	{
		if (!m_entity.Any())
			html.H1("Entity not found");
		else
		{
			if (m_nrEntitiesRemoved || m_nrEntitiesInserted)
				if (AbortTxOnSuccess())
					html.P().B().UInt(m_nrEntitiesRemoved).EndB().T(" entities would have been removed, and ").B().UInt(m_nrEntitiesInserted).EndB().T(" entities inserted, before an error occurred.").EndP();
				else
					html.P().B().UInt(m_nrEntitiesRemoved ).EndB().T(" entities removed.").EndP()
						.P().B().UInt(m_nrEntitiesInserted).EndB().T(" entities inserted.").EndP();

			Str browsePath = DAP_Env_MountPath();
			browsePath.Add("browse?entityId=").UrlEncode(Str::From(m_entity->m_entityId));

			html.H1().A().Href(Str(DAP_Env_MountPath()).Add("browse?entityId=").UrlEncode(Str::From(m_entity->m_entityId))).T(m_entity->GetKindName()).T(" ").T(Str::From(m_entity->m_entityId)).EndA().EndH1();
		
			if (m_entity->m_parentId == ObjId::None)
				html.P().T("Root entity").EndP();
			else
			{
				Str parentIdStr;
				parentIdStr.Obj(m_entity->m_parentId);
				html.P().T("Child of ").A().Href(Str(DAP_Env_MountPath()).Add("browse?entityId=").UrlEncode(parentIdStr)).T(parentIdStr).EndA().EndP();
			}

			bool jsonBeingEdited = m_editedEntityJson.Any();
			Str displayEntityJson;
			if (jsonBeingEdited)
				displayEntityJson = m_editedEntityJson;
			else
			{
				Str entityJsonRaw;
				m_entity->JsonEncode(entityJsonRaw);

				ParseTree parseTree(entityJsonRaw);
				EnsureThrow(parseTree.Parse(Json::C_Json));
				displayEntityJson = Json::PrettyPrint(parseTree.Root());
			}

			html.SideEffectForm(browsePath, "saveForm")
					.P().A().Id("editLink").Href("#").T(jsonBeingEdited ? "Edit" : "Cancel").EndA().EndP()
					.TextArea().IdAndName("entityJson").Rows("20").Cols("120").DisabledIf(!jsonBeingEdited)
						.T(displayEntityJson)
					.EndTextArea()
					.Div().InputSubmit("cmd", c_zCmdSaveEntity).Id("entitySave").DisabledIf(!jsonBeingEdited).EndDiv()
				.EndForm();

			html.AddJs(c_js_AtDbAdm_Browse);

			// Display children
			html.H2().T("Children").EndH2();

			if (m_children.Any())
			{
				html.Div().Id("children")
					.Table().Class("border");

				uint32 curKind = m_children[0].m_kind;
				sizet curKindStart = 0;
				sizet curKindEnd = 1;

				while (true)
				{
					if (curKindEnd == m_children.Len() || m_children[curKindEnd].m_kind != curKind)
					{
						Entity const& sampleEntity = GetEntitySample(curKind);
						sizet nrChildrenOfKind = curKindEnd - curKindStart;

						html.Tr()
								.Td().T(sampleEntity.GetKindName()).EndTd()
								.Td().UInt(nrChildrenOfKind).EndTd()
								.Td().T("Key").EndTd()
								.Td().T("ObjId").EndTd()
							.EndTr();

						for (EntityChildInfo const& eci : m_children.GetSlice(curKindStart, nrChildrenOfKind))
						{
							Str childIdStr;
							childIdStr.Obj(eci.m_entityId);
							html.Tr()
									.Td().EndTd()
									.Td().EndTd()
									.Td().T(eci.JsonEncodeKey()).EndTd()
									.Td().A().Href(Str(DAP_Env_MountPath()).Add("browse?entityId=").UrlEncode(childIdStr)).T(childIdStr).EndA().EndTd()
								.EndTr();
						}

						if (curKindEnd == m_children.Len())
							break;

						curKindStart = curKindEnd;
						curKind = m_children[curKindStart].m_kind;
					}

					++curKindEnd;
				}

				html    .EndTable()
					.EndDiv();
			}

			html.H1("Import script")
				.MultipartForm(browsePath, "importForm")
					.Table()
						.Tr()
							.Td().P().Label("file", "Import script from file").EndP().EndTd()
							.Td()
								.P().InputFile().IdAndName("file").EndP()
								.P().Class("help").T("Specify a file containing an import script with instructions encoded as a JSON array.").EndP()
							.EndTd()
						.EndTr()
						.Tr()
							.Td().EndTd()
							.Td().InputSubmit("cmd", c_zCmdImportScript).EndTd()
						.EndTr()
					.EndTable()
				.EndForm();

			html.H1("Remove entity")
				.SideEffectForm(browsePath)
					.P().ConfirmSubmit("Confirm removal", "cmd", c_zCmdRemoveEntity).EndP()
				.EndForm();
		}
		return html;
	}
}
